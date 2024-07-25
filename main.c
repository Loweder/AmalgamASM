#include "main.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define check_hw_ol(type) if (desc->md_count[PORT_ ## type] > hw_limits[desc->opts & OPT_COMPLEX][PORT_ ## type]) p->status |= ERR_ ## type ## _OL
#define check_fq(against) if (p->top_freq < (against)) p->top_freq = (against)
#define ret_ints(intp) int_simple(s, core, INT_ ## intp); \
  return 0;
#define ret_intc(intp) int_complex(s, core, INT_ ## intp); \
  return 0;
#define ifln(flag) if (REG_LN(insn[flag]))

//TODO make use of new MACROs
hw *build_system(hw_desc *desc) {
  hw *p = malloc(sizeof(hw));
  if (!p) return 0;

  p->status = 0;
  p->top_freq = 0;

  check_hw_ol(MAIN);
  check_hw_ol(ROM);
  check_hw_ol(RAM);
  check_hw_ol(IO);

  if ((!(desc->opts & OPT_COMPLEX) && desc->cpu_count > 1) || desc->cpu_count > 8)
    p->status |= ERR_CPU_OL;
  else {
    p->cpu_count = desc->cpu_count;
    p->cpus = malloc(sizeof(cpu)*p->cpu_count);
    if (!p->cpus) p->status |= ERR_MALLOC;
    else {
      for (int i = 0; i < p->cpu_count; i++) {
	cpu *core = p->cpus + i;
	core->max_freq = core->freq = desc->cpus[i].freq;
	check_fq(core->freq);
	for (int j = 0; j < GPR_LAST; j++) core->gpr[j] = 0;
	for (int j = 0; j < MSR_LAST; j++) core->msr[j] = 0;
	core->msr[MSR_MODE] = (i << MSR_MODE_COREID) & MSR_MODE_COREID_MASK;
      }
      p->cpus[0].gpr[GPR_IP] = 0x1100;
      p->cpus[0].msr[MSR_MODE] |= 1 << MSR_MODE_RUNNING;
    }
  }

  if (!(p->status & ERR_RAM_OL)) {
    p->ram_size = 0;
    p->ram_freq = 0;
    for (int i = 0; i < desc->md_count[PORT_RAM]; i++) {
      md_desc *ram_bank = desc->md[PORT_RAM] + i;
      if (p->ram_freq && p->ram_freq != ram_bank->freq) p->status |= ERR_RAM_HZ;
      if (ram_bank->p_size > 16*1024*1024) p->status |= ERR_RAM_MAX;
      p->ram_freq = ram_bank->freq;
      p->ram_size += ram_bank->p_size;
      check_fq(ram_bank->freq);
    }
    if (!(p->status & ERR_RAM_MAX)) {
      if (p->ram_size > 0x2000) {
	p->ram = malloc(desc->opts & OPT_COMPLEX ? p->ram_size : (p->ram_size > 48*1024 ? 48*1024 : p->ram_size));
	if (!p->ram) p->status |= ERR_MALLOC;
      } else {
	p->status |= ERR_RAM_MIN;
      }
    }
  }

  if (!(p->status & ERR_MAIN_OL)) {
    p->disk_count = desc->md_count[PORT_MAIN];
    for (int i = 0; i < p->disk_count; i++) {
      md_desc *disk = desc->md[PORT_MAIN] + i;
      p->disk_freq[i] = disk->freq;
      p->disk_size[i] = disk->p_size;
      p->disks[i] = disk->builder();
      check_fq(disk->freq);
      if (!p->disks[i]) p->status |= ERR_MALLOC;
    }
  }

  const uint8_t *boot_rom = 0;
  if (!(p->status & ERR_ROM_OL)) {
    p->rom_count = desc->md_count[PORT_ROM];
    for (int i = 0; i < p->rom_count; i++) {
      md_desc *rom = desc->md[PORT_ROM] + i;
      p->rom_size[i] = rom->p_size;
      p->roms[i] = rom->builder();
      if (!p->roms[i]) p->status |= ERR_MALLOC;
      else if (rom->p_size > 0 && rom->p_size % 4096 == 0 && *((uint32_t*)p->roms[i]) == 0xAC3618CA)
	boot_rom = p->roms[i];
    }
  }
  if (!boot_rom) p->status |= ERR_CPU_IP;

  if (!(p->status & ERR_IO_OL)) {
    p->io_count = desc->md_count[PORT_IO];
    for (int i = 0; i < p->io_count; i++) {
      md_desc *io = desc->md[PORT_IO] + i;
      p->io_freq[i] = io->freq;
      p->io_proc[i] = io->processor;
      check_fq(io->freq);
    }
  }

  if (!(p->status & 0xFFFF)) {
    p->status |= STATE_RUNNING | STATE_STABLE;
    memcpy(p->ram + 0x1000, boot_rom, 0x1000);
  }

  return p;
}
//FIXME potential SEGV when reading last few bytes
uint8_t *mmu_simple(hw *s, cpu *core, uint16_t addr) {
  if (s->ram_size > addr) return s->ram + addr;
  ret_ints(MR)
}

uint8_t *mmu_complex(hw *s, cpu *core, uint32_t addr, uint8_t task) {
  uint32_t paging = MSR(PAGING);
  uint32_t ram_size = s->ram_size;
  task = ((task << 1) & PG_MODE) | (BIT_G(~MSR(MODE), MSR_MODE_KERNEL) << 3);

  if ((M10(paging) + F10) >= ram_size) {ret_intc(MR)}
  uint32_t page_dir = *(uint32_t*) (s->ram + (M10(paging) + PG_MMU_PD(addr)));

  if (!(page_dir & PG_AVAIL)) {MSR(PAGE_FAULT) = addr; ret_intc(PF)}
  if ((page_dir & task & PG_SUPERV) != (task & PG_MODE)) {ret_intc(GP)}
  if ((page_dir & task & PG_SUPERV) != (page_dir & PG_SUPERV)) {ret_intc(GP)}

  if ((M10(page_dir) + F10) >= ram_size) {ret_intc(MR)}
  uint32_t page_entry = *(uint32_t*) (s->ram + (M10(page_dir) + PG_MMU_PE(addr)));

  if (!(page_entry & PG_AVAIL)) {MSR(PAGE_FAULT) = addr; ret_intc(PF)}
  if ((page_entry & task & PG_SUPERV) != (task & PG_MODE)) {ret_intc(GP)}
  if ((page_entry & task & PG_SUPERV) != (page_entry & PG_SUPERV)) {ret_intc(GP)}

  uint8_t dev = (page_entry & PG_DEV) >> 4;

  if (!dev) {
    if ((M10(page_entry) + F10) >= ram_size) {ret_intc(MR)}
    return s->ram + (M10(page_entry) + PG_MMU_ADDR(addr));
  }
  if (page_entry & 0x2) {
    if (dev >= s->disk_count || ((M10(page_entry) + F10) >= s->disk_size[dev])) {ret_intc(MR)}
    return s->disks[dev] + (M10(page_entry) + PG_MMU_ADDR(addr)); 
  } else {
    if (dev >= s->rom_count || ((M10(page_entry) + F10) >= s->rom_size[dev])) {ret_intc(MR)}
    return (uint8_t*) s->roms[dev] + (M10(page_entry) + PG_MMU_ADDR(addr)); 
  }
}

void int_simple(hw *s, cpu *core, uint8_t number) {
  if (!BIT_G(MSR(ALU), MSR_ALU_IF)) return;
  BIT_R(MSR(ALU), MSR_ALU_IF);
  uint16_t *ivt = (uint16_t*) s->ram;
  uint16_t *stack = MMUS_16(GPR(SP) -= 2);
  if (!stack) {
    BIT_R(MSR(MODE), MSR_MODE_RUNNING);
    return;
  }
  *stack = GPR(IP);
  GPR(IP) = ivt[number*2];
}

void int_complex(hw *s, cpu *core, uint8_t number) {
  if (!BIT_G(MSR(ALU), MSR_ALU_IF)) return;
  uint32_t intp = MSR(INT);
  BIT_R(MSR(ALU), MSR_ALU_IF);
  uint32_t *idt = MMUC_32(M10(intp), MMU_READ);
  uint32_t *stack = MMUC_32(GPR(SP) -= 4, MMU_WRITE);
  if (!idt || !stack) {
    BIT_R(MSR(MODE), MSR_MODE_RUNNING);
    return;
  }
  *stack = GPR(IP);
  GPR(IP) = *(idt + number);
  BIT_M(MSR(INT), MSR_INT_KERNEL, BIT_G(MSR(MODE), MSR_MODE_KERNEL));
  BIT_S(MSR(MODE), MSR_MODE_KERNEL);
}

void execute_simple(hw *s, cpu *core) {
  uint8_t *insn = MMUS_8(GPR(IP));
  if (!insn) return;
  uint8_t size = 1;
  switch (insn[0]) {
    case I_NOP:
      size = 1;
      break;


    case I_MOV:
      switch(REG_RM(insn[1])) {
	case 0x10:
	  ifln(1) GPR_32(1) = *MMUS_32(GPR_16(2));
	  else GPR_16(1) = *MMUS_16(GPR_16(2));
	  break;
	case 0x20:
	  ifln(1) *MMUS_32(GPR_16(1)) = GPR_32(2); 
	  else *MMUS_16(GPR_16(1)) = GPR_16(2);
	  break;
	default:
	  ifln(1) GPR_32(1) = GPR_32(2); 
	  else GPR_16(1) = GPR_16(2);
	  break;
      }
      size = 3;
      break;
    case I_MOVI:
      switch(REG_RM(insn[1])) {
	case 0x20:
	  *MMUS_16(GPR_16(1)) = IMM_16(2);
	  break;
	default:
	  GPR_16(1) = IMM_16(2);
	  break;
      }
      size = 4;
      break;
    case I_SWAP:
      ifln(1) {
	uint32_t imm = GPR_32(2);
	GPR_32(2) = GPR_32(1);
	GPR_32(1) = imm;
      } else {
	uint16_t imm = GPR_16(2);
	GPR_16(2) = GPR_16(1);
	GPR_16(1) = imm;      
      }
      size = 3;
      break;
    case I_PUSH:
      ifln(1) *MMUS_32(GPR(SP) -= 4) = GPR_32(1);
      else *MMUS_16(GPR(SP) -= 2) = GPR_16(1);
      size = 2;
      break;
    case I_POP:
      ifln(1) GPR_32(1) = *MMUS_32(GPR(SP) -= 4);
      else GPR_16(1) = *MMUS_16(GPR(SP) -= 2);
      size = 2;
      break;


    case I_ADD:
      size = 3;
      break;
    case I_ADI:
      size = 4;
      break;
    case I_SUB:
      size = 3;
      break;
    case I_SBI:
      size = 4;
      break;

    case I_MUL:
      size = 3;
      break;
    case I_DIV:
      size = 3;
      break;
    case I_MOD:
      size = 3;
      break;
    case I_INC:
      size = 2;
      break;
    case I_DEC:
      size = 2;
      break;

    case I_XOR:
      size = 3;
      break;
    case I_XORI:
      size = 4;
      break;
    case I_OR:
      size = 3;
      break;
    case I_ORI:
      size = 4;
      break;
    case I_AND:
      size = 3;
      break;
    case I_ANDI:
      size = 4;
      break;
    case I_NOT:
      size = 2;
      break;

    case I_BTS:
      size = 3;
      break;
    case I_BTR:
      size = 3;
      break;
    case I_BTC:
      size = 3;
      break;

    case I_SHL:
      size = 3;
      break;
    case I_SHR:
      size = 3;
      break;
    case I_ROL:
      size = 3;
      break;
    case I_ROR:
      size = 3;
      break;


    case I_INT:
      size = 2;
      break;
    case I_JMP:
      size = 3;
      break;
    case I_IJMP:
      size = 2;
      break;
    case I_JFL:
      size = 4;
      break;
    case I_RET:
      size = 1;
      break;


    case I_IO:
      size = 4;
      break;
    case I_IIO:
      size = 3;
      break;
    case I_CR:
      size = 3;
      break;

    case I_SEI:
      size = 1;
      break;
    case I_CLI:
      size = 1;
      break;
    case I_IRET:
      size = 1;
      break;
    default:
      int_simple(s, core, INT_IL);
      return;
  }
  GPR(IP) += size;
}

void execute_complex(hw *s, cpu *core) {
  uint8_t *insn = MMUC_8(GPR(IP), MMU_EXEC);
  if (!insn) return;
  if (insn[0] & 0x80 && !BIT_G(MSR(MODE), MSR_MODE_KERNEL)) {
    int_complex(s, core, INT_GP);
    return;
  }
  uint8_t size = 1;
  switch (insn[0]) {
    case I_NOP:
      size = 1;
      break;


    case I_MOV:
      switch(REG_RM(insn[1])) {
	case 0x10:
	  break;
	case 0x20:
	  break;
	default:
	  //GPRI(REG_OR(insn[1])) = GPRI(REG_OR(insn[2]));
	  break;
      }
      size = 3;
      break;
    case I_MOVI:
      break;
    case I_FMOVI:
      break;
    case I_SWAP:
      break;
    case I_STACK:
      size = 2;
      break;


    case I_ADD:
      size = 3;
      break;
    case I_ADI:
      break;
    case I_SUB:
      size = 3;
      break;
    case I_SBI:
      break;

    case I_MUL:
      size = 3;
      break;
    case I_DIV:
      size = 3;
      break;
    case I_MOD:
      size = 3;
      break;
    case I_INC:
      size = 2;
      break;
    case I_DEC:
      size = 2;
      break;

    case I_FADD:
      size = 3;
      break;
    case I_FSUB:
      size = 3;
      break;
    case I_FMUL:
      size = 3;
      break;
    case I_FDIV:
      size = 3;
      break;

    case I_I2F:
      size = 2;
      break;
    case I_F2I:
      size = 2;
      break;

    case I_XOR:
      size = 3;
      break;
    case I_XORI:
      break;
    case I_OR:
      size = 3;
      break;
    case I_ORI:
      break;
    case I_AND:
      size = 3;
      break;
    case I_ANDI:
      break;
    case I_NOT:
      size = 2;
      break;

    case I_BTS:
      size = 3;
      break;
    case I_BTR:
      size = 3;
      break;
    case I_BTC:
      size = 3;
      break;

    case I_SHL:
      size = 3;
      break;
    case I_SHR:
      size = 3;
      break;
    case I_ROL:
      size = 3;
      break;
    case I_ROR:
      size = 3;
      break;


    case I_INT:
      size = 2;
      break;
    case I_JMP:
      break;
    case I_IJMP:
      break;
    case I_JFL:
      break;
    case I_RET:
      size = 1;
      break;


    case I_IO:
      size = 4;
      break;
    case I_IIO:
      size = 3;
      break;
    case I_CR:
      size = 3;
      break;

    case I_SEI:
      size = 1;
      break;
    case I_CLI:
      size = 1;
      break;
    case I_IRET:
      size = 1;
      break;
    default:
      int_complex(s, core, INT_IL);
      return;
  }
  GPR(IP) += size;
}

void execute(hw *s) {
  for (int i = 0; i < s->cpu_count; i++) {
    cpu *core = s->cpus + i;
    if (BIT_G(MSR(MODE), MSR_MODE_RUNNING)) {
      if (BIT_G(MSR(MODE), MSR_MODE_COMPLEX))
	execute_complex(s, core);
      else
	execute_simple(s, core);
    }
  } 
}

int main(void) {

}

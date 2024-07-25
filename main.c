#include "main.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define check_hw_ol(type) if (desc->md_count[PORT_ ## type] > hw_limits[desc->opts & OPT_COMPLEX][PORT_ ## type]) p->status |= ERR_ ## type ## _OL
#define check_fq(against) if (p->top_freq < (against)) p->top_freq = (against)
#define ret_int(intp, value) interrupt(s, core, INT_ ## intp); \
  return value; \

void interrupt(hw *s, cpu *core, uint8_t number);

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
uint8_t *mmu_map(hw *s, cpu *core, uint32_t addr, uint8_t task) {
  uint32_t mode = MSR(MODE), paging = MSR(PAGING);
  uint32_t ram_size = s->ram_size;
  
  if (!BIT_G(mode, MSR_MODE_COMPLEX)) {
    if (ram_size > addr) return RAM_8(addr);
    ret_int(MR, 0)
  }
  
  if ((M10(paging) + F10) >= ram_size) {ret_int(MR, 0)}
  uint32_t page_dir = *RAM_32(M10(paging) + PG_MMU_PD(addr));
  
  task = ((task << 1) & PG_MODE) | ((~mode >> (MSR_MODE_KERNEL-3)) & PG_SUPERV);
  if (!(page_dir & PG_AVAIL)) {MSR(PAGE_FAULT) = addr; ret_int(PF, 0)}
  if ((page_dir & task & PG_SUPERV) != (task & PG_MODE)) {ret_int(GP, 0)}
  if ((page_dir & task & PG_SUPERV) != (page_dir & PG_SUPERV)) {ret_int(GP, 0)}
  
  if ((M10(page_dir) + F10) >= ram_size) {ret_int(MR, 0)}
  uint32_t page_entry = *RAM_32(M10(page_dir) + PG_MMU_PE(addr));
  
  if (!(page_entry & PG_AVAIL)) {MSR(PAGE_FAULT) = addr; ret_int(PF, 0)}
  if ((page_entry & task & PG_SUPERV) != (task & PG_MODE)) {ret_int(GP, 0)}
  if ((page_entry & task & PG_SUPERV) != (page_entry & PG_SUPERV)) {ret_int(GP, 0)}

  uint8_t dev = (page_entry & PG_DEV) >> 4;
  
  if (!dev) {
    if ((M10(page_entry) + F10) >= ram_size) {ret_int(MR, 0)}
    return RAM_8(M10(page_entry) + PG_MMU_ADDR(addr));
  }
  if (page_entry & 0x2) {
    if (dev >= s->disk_count || ((M10(page_entry) + F10) >= s->disk_size[dev])) {ret_int(MR, 0)}
    return MEM_8(disks[dev], M10(page_entry) + PG_MMU_ADDR(addr)); 
  } else {
    if (dev >= s->rom_count || ((M10(page_entry) + F10) >= s->rom_size[dev])) {ret_int(MR, 0)}
    return MEM_8(roms[dev], M10(page_entry) + PG_MMU_ADDR(addr)); 
  }
}

void interrupt(hw *s, cpu *core, uint8_t number) {
  if (!BIT_G(MSR(ALU), MSR_ALU_IF)) return;
  uint32_t mode = MSR(MODE), intp = MSR(INT);
  BIT_C(MSR(ALU), MSR_ALU_IF)
  if (!BIT_G(mode, MSR_MODE_COMPLEX)) {
    uint16_t sp = (GPR(SP) -= 2);
    if (sp + 2 >= s->ram_size) {
      BIT_C(MSR(MODE), MSR_MODE_RUNNING)
      return;
    }
    *RAM_16(sp) = GPR(IP);
    GPR(IP) = *RAM_16(number*2);
  } else {
    uint32_t *idt = (uint32_t*) mmu_map(s, core, M10(intp), MMU_READ);
    uint32_t *stack = (uint32_t*) mmu_map(s, core, GPR(SP) -= 4, MMU_WRITE);
    if (!idt || !stack) {
      BIT_C(MSR(MODE), MSR_MODE_RUNNING)
      return;
    }
    *stack = GPR(IP);
    GPR(IP) = *(idt + number);
    BIT_M(MSR(INT), MSR_INT_KERNEL, BIT_G(MSR(MODE), MSR_MODE_KERNEL))
    BIT_S(MSR(MODE), MSR_MODE_KERNEL)
  }
}

void execute_core(hw *s, cpu *core) {
  uint8_t *insn = mmu_map(s, core, GPR(IP), MMU_EXEC);
  if (!insn) return;
  if (insn[0] & 0x80 && !BIT_G(MSR(MODE), MSR_MODE_KERNEL)) {
    interrupt(s, core, INT_GP);
    return;
  }
  uint8_t size = 1;
  switch (insn[0]) {
    case I_NOP:
      size = 1;
      break;

  
    case I_MOV:
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
      interrupt(s, core, INT_IL);
      return;
  }
  GPR(IP) += size;
}

void execute(hw *s) {
  for (int i = 0; i < s->cpu_count; i++) {
    cpu *core = s->cpus + i;
    if (core->msr[MSR_MODE] & (1 << MSR_MODE_RUNNING))
      execute_core(s, core);
  } 
}

int main(void) {

}

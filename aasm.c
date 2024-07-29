#include "aasm/aasm.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define check_hw_ol(type) MDD_TEST_LIMIT(type) p->status |= HW_ ## type ## _OL
#define ret_ints(intp) int_simple(s, core, INT_ ## intp); \
  return 0;
#define ret_intc(intp) int_complex(s, core, INT_ ## intp); \
  return 0;
#define ret_intm(intp) if (core->status & CPU_COMPLEX) int_complex(s, core, INT_ ## intp); \
  else int_simple(s, core, INT_ ## intp); \
  return 0;
#define ifln(flag) if (REG_LN(insn[flag]))
#define ifmem(flag) if (REG_RM(insn[flag]))

hw *build_system(const hw_desc *desc) {
  hw *p = malloc(sizeof(hw));
  if (!p) return 0;
  const uint8_t *boot_rom = 0;

  p->status = 0;

  check_hw_ol(CPU);
  check_hw_ol(MAIN);
  check_hw_ol(ROM);
  check_hw_ol(RAM);
  check_hw_ol(IO);

  if (!(p->status & HW_RAM_OL)) {
    p->ram_size = 0;
    for (int i = 0; i < desc->md_count[MDD_PORT_RAM]; i++) {
      md_desc *ram_bank = desc->md[MDD_PORT_RAM] + i;
      //if (p->ram_freq && p->ram_freq != ram_bank->freq) p->status |= HW_RAM_HZ;
      if (ram_bank->p_size > 16*1024*1024) p->status |= HW_RAM_MAX;
      if (ram_bank->p_size % 1024) p->status |= HW_RAM_MALF;
      p->ram_size += ram_bank->p_size;
    }
    if (!(p->status & (HW_RAM_MAX | HW_RAM_HZ | HW_RAM_MALF))) {
      if (p->ram_size > 0x2000) {
	p->ram = malloc(desc->opts & HWD_OPT_COMPLEX ? p->ram_size : (p->ram_size > 48*1024 ? 48*1024 : p->ram_size));
	if (!p->ram) p->status |= HW_MALLOC;
      } else {
	p->status |= HW_RAM_MIN;
      }
    }
  }

  if (!(p->status & HW_MAIN_OL)) {
    p->disk_count = desc->md_count[MDD_PORT_MAIN];
    for (int i = 0; i < p->disk_count; i++) {
      md_desc *disk = desc->md[MDD_PORT_MAIN] + i;
      if (disk->p_size % 1024) p->status |= HW_MAIN_MALF;
      p->disk_size[i] = disk->p_size;
      p->disks[i] = disk->builder();
      if (!p->disks[i]) p->status |= HW_MALLOC;
    }
  }

  if (!(p->status & HW_ROM_OL)) {
    p->rom_count = desc->md_count[MDD_PORT_ROM];
    for (int i = 0; i < p->rom_count; i++) {
      md_desc *rom = desc->md[MDD_PORT_ROM] + i;
      if (rom->p_size % 1024) p->status |= HW_ROM_MALF;
      p->rom_size[i] = rom->p_size;
      p->roms[i] = rom->builder();
      if (!p->roms[i]) p->status |= HW_MALLOC;
      else if (rom->p_size > 0 && rom->p_size % 4096 == 0 && *((uint32_t*)p->roms[i]) == 0xCA1836AC)
	boot_rom = p->roms[i];
    }
  }

  if (!(p->status & HW_IO_OL)) {
    p->io_count = desc->md_count[MDD_PORT_IO];
    for (int i = 0; i < p->io_count; i++) {
      md_desc *io = desc->md[MDD_PORT_IO] + i;
      p->io_size[i] = io->p_size;
      p->io_proc[i] = io->processor;
      p->ios[i] = io->builder();
      if (!p->ios[i]) p->status |= HW_MALLOC;
    }
  }

  if (!(p->status & HW_CPU_OL)) {
    p->cpu_count = desc->md_count[MDD_PORT_CPU];
    for (int i = 0; i < p->cpu_count; i++) {
      cpu *core = p->cpus + i;
      core->status = 0;
      core->max_freq = core->freq = desc->md[MDD_PORT_CPU][i].freq;
      for (int j = 0; j < GPR_LAST; j++) core->gpr[j] = 0;
      for (int j = 0; j < MSR_LAST; j++) core->msr[j] = 0;
      MSR(MODE) = (i << MSR_MODE_COREID) & MSR_MODE_COREID_MASK;
      MSR(MODE) |= (1 << MSR_MODE_KERNEL);
      GPR(SP) = p->ram_size;
      GPR(BP) = p->ram_size;
    }
    p->cpus[0].status |= CPU_RUNNING;
    p->cpus[0].gpr[GPR_IP] = 0x1100;
    p->cpus[0].msr[MSR_MODE] |= (1 << MSR_MODE_RUNNING);
  }
 
  p->hw_size = desc->p_size;
  p->hw_proc = desc->processor;
  p->hw_space = desc->builder();
  if (!p->hw_space) p->status |= HW_MALLOC;

  if (!boot_rom) p->status |= HW_CPU_IP;
  else if (!(p->status & 0xFFFF)) {
    p->status |= HW_RUNNING | HW_STABLE;
    memcpy(p->ram + 0x1000, boot_rom, 0x1000);
  }
  
  if (desc->opts & HWD_OPT_COMPLEX) p->status |= HW_COMPLEX;

  return p;
}

proc mmu_iopc(hw *s, cpu *core, uint16_t addr) {
  uint8_t segment = addr / 0x1000;
  if (segment > s->io_count) {ret_intm(IO)}
  if (segment == 0) return s->hw_proc;
  return s->io_proc[segment - 1];
}

uint8_t *mmu_io(hw *s, cpu *core, uint16_t addr, uint8_t size) {
  uint8_t segment = addr / 0x1000, seg_size, *seg_data;
  if (segment > s->io_count) {ret_intm(IO)}
  if (segment == 0) {seg_size = s->hw_size; seg_data = s->hw_space; }
  else {seg_size = s->io_size[segment - 1]; seg_data = s->ios[segment - 1]; }
  if (seg_size >= (addr % 0x1000) + size) return seg_data + (addr % 0x1000);
  ret_intm(IO);
}

uint8_t *mmu_simple(hw *s, cpu *core, uint16_t addr, uint8_t size) {
  if (s->ram_size >= addr + size) return s->ram + addr;
  ret_ints(MR)
}
//FIXME segv on page edge access
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
  if ((!BIT_G(MSR(ALU), MSR_ALU_IF) && number >= 0x20) || core->status & CPU_INT_R_FS) return;
  core->status |= CPU_INT_R_FS;
  BIT_R(MSR(ALU), MSR_ALU_IF);
  if (core->status & CPU_ASLEEP) {
    core->status &= ~CPU_ASLEEP;
    core->sleep = 0;
  }
  uint16_t *ivt = (uint16_t*) s->ram;
  uint16_t *stack = MMUS_16(GPR(SP) -= 2);
  if (!stack) {
    core->status |= CPU_INT_FAIL;
    core->status &= ~(CPU_RUNNING | CPU_INT_R_FS);
    BIT_R(MSR(MODE), MSR_MODE_RUNNING);
    return;
  }
  *stack = GPR(IP);
  GPR(IP) = ivt[number*2];
  core->status &= ~CPU_INT_R_FS;
}

void int_complex(hw *s, cpu *core, uint8_t number) {
  if ((!BIT_G(MSR(ALU), MSR_ALU_IF) && number >= 0x20) || core->status & CPU_INT_R_FS) return;
  core->status |= CPU_INT_R_FS;
  uint32_t intp = MSR(INT);
  BIT_R(MSR(ALU), MSR_ALU_IF);
  if (core->status & CPU_ASLEEP) {
    core->status &= ~CPU_ASLEEP;
    core->sleep = 0;
  }
  uint32_t *idt = MMUC_32(M10(intp), MMU_READ);
  uint32_t *stack = MMUC_32(GPR(SP) -= 4, MMU_WRITE);
  if (!idt || !stack) {
    core->status |= CPU_INT_FAIL;
    core->status &= ~(CPU_RUNNING | CPU_INT_R_FS);
    BIT_R(MSR(MODE), MSR_MODE_RUNNING);
    return;
  }
  *stack = GPR(IP);
  GPR(IP) = *(idt + number);
  BIT_M(MSR(INT), MSR_INT_KERNEL, BIT_G(MSR(MODE), MSR_MODE_KERNEL));
  BIT_S(MSR(MODE), MSR_MODE_KERNEL);
  core->status &= ~CPU_INT_R_FS;
}

void execute_simple(hw *s, cpu *core) {
  uint8_t *insn = mmu_simple(s, core, GPR(IP), 8);
  if (!insn) return;
  uint8_t size = 1;
  uint32_t sleep = 0;
  switch (insn[0]) {
    case I_NOP:
      size = 1;
      break;
    case I_SLEEP:
      core->status |= CPU_ASLEEP;
      ifmem(1) sleep = *MMUS_16(GPR_16(1));
      else sleep = GPR_16(1);
      size = 2;
      break;
    case I_SLEEPI:
      core->status |= CPU_ASLEEP;
      sleep = IMM_16(1);
      size = 3;
      break;
    case I_GIPC:
      ifmem(1) *MMUS_32(GPR_16(1)) = core->ip_count;
      else GPR_32(1) = core->ip_count;
      size = 2;
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
      ifmem(1) *MMUS_16(GPR_16(1)) = IMM_16(2);
      else GPR_16(1) = IMM_16(2);
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
      ifln(1) {GPR_32(1) = *MMUS_32(GPR(SP)); GPR(SP) += 4;}
      else {GPR_16(1) = *MMUS_16(GPR(SP)); GPR(SP) += 2;}
      size = 2;
      break;


    case I_ADD:
      switch(REG_RM(insn[1])) {
	case 0x10:
	  ifln(1) GPR_32(1) += *MMUS_32(GPR_16(2));
	  else GPR_16(1) += *MMUS_16(GPR_16(2));
	  break;
	case 0x20:
	  ifln(1) *MMUS_32(GPR_16(1)) += GPR_32(2); 
	  else *MMUS_16(GPR_16(1)) += GPR_16(2);
	  break;
	default:
	  ifln(1) GPR_32(1) += GPR_32(2); 
	  else GPR_16(1) += GPR_16(2);
	  break;
      }
      size = 3;
      break;
    case I_ADI:
      ifmem(1) *MMUS_16(GPR_16(1)) += IMM_16(2);
      else GPR_16(1) += IMM_16(2);
      size = 4;
      break;
    case I_SUB:
      switch(REG_RM(insn[1])) {
	case 0x10:
	  ifln(1) GPR_32(1) -= *MMUS_32(GPR_16(2));
	  else GPR_16(1) -= *MMUS_16(GPR_16(2));
	  break;
	case 0x20:
	  ifln(1) *MMUS_32(GPR_16(1)) -= GPR_32(2); 
	  else *MMUS_16(GPR_16(1)) -= GPR_16(2);
	  break;
	default:
	  ifln(1) GPR_32(1) -= GPR_32(2); 
	  else GPR_16(1) -= GPR_16(2);
	  break;
      }
      size = 3;
      break;
    case I_SBI:
      ifmem(1) *MMUS_16(GPR_16(1)) -= IMM_16(2);
      else GPR_16(1) -= IMM_16(2);
      size = 4;
      break;

    case I_MUL:
      switch(REG_RM(insn[1])) {
	case 0x10:
	  GPR_32(1) *= *MMUS_16(GPR_16(2));
	  break;
	case 0x20:
	  *MMUS_32(GPR_16(1)) *= GPR_16(2);
	  break;
	default:
	  GPR_32(1) *= GPR_16(2);
	  break;
      }
      sleep = 1;
      size = 3;
      break;
    case I_DIV:
      switch(REG_RM(insn[1])) {
	case 0x10:
	  ifln(1) GPR_32(1) /= *MMUS_32(GPR_16(2));
	  else GPR_16(1) /= *MMUS_16(GPR_16(2));
	  break;
	case 0x20:
	  ifln(1) *MMUS_32(GPR_16(1)) /= GPR_32(2);
	  else *MMUS_16(GPR_16(1)) /= GPR_16(2);
	  break;
	default:
	  ifln(1) GPR_32(1) /= GPR_32(2);
	  else GPR_16(1) /= GPR_16(2);
	  break;
      }
      sleep = 7;
      size = 3;
      break;
    case I_MOD:
      switch(REG_RM(insn[1])) {
	case 0x10:
	  ifln(1) GPR_32(1) %= *MMUS_32(GPR_16(2));
	  else GPR_16(1) %= *MMUS_16(GPR_16(2));
	  break;
	case 0x20:
	  ifln(1) *MMUS_32(GPR_16(1)) %= GPR_32(2);
	  else *MMUS_16(GPR_16(1)) %= GPR_16(2);
	  break;
	default:
	  ifln(1) GPR_32(1) %= GPR_32(2);
	  else GPR_16(1) %= GPR_16(2);
	  break;
      }
      sleep = 7;
      size = 3;
      break;
    case I_INC:
      switch(REG_LN(insn[1])) {
	case 0x40:
	  ifln(1) (*MMUS_32(GPR_16(1)))++;
	  else (GPR_32(1))++;
	  break;
	default:
	  ifln(1) (*MMUS_16(GPR_16(1)))++;
	  else (GPR_16(1))++;
	  break;
      }
      size = 2;
      break;
    case I_DEC:
      switch(REG_LN(insn[1])) {
	case 0x40:
	  ifmem(1) (*MMUS_32(GPR_16(1)))--;
	  else (GPR_32(1))--;
	  break;
	default:
	  ifmem(1) (*MMUS_16(GPR_16(1)))--;
	  else (GPR_16(1))--;
	  break;
      }
      size = 2;
      break;

    case I_XOR:
      switch(REG_RM(insn[1])) {
	case 0x10:
	  ifln(1) GPR_32(1) ^= *MMUS_32(GPR_16(2));
	  else GPR_16(1) ^= *MMUS_16(GPR_16(2));
	  break;
	case 0x20:
	  ifln(1) *MMUS_32(GPR_16(1)) ^= GPR_32(2); 
	  else *MMUS_16(GPR_16(1)) ^= GPR_16(2);
	  break;
	default:
	  ifln(1) GPR_32(1) ^= GPR_32(2); 
	  else GPR_16(1) ^= GPR_16(2);
	  break;
      }
      size = 3;
      break;
    case I_XORI:
      ifmem(1) *MMUS_16(GPR_16(1)) ^= IMM_16(2);
      else GPR_16(1) ^= IMM_16(2);
      size = 4;
      break;
    case I_OR:
      switch(REG_RM(insn[1])) {
	case 0x10:
	  ifln(1) GPR_32(1) |= *MMUS_32(GPR_16(2));
	  else GPR_16(1) |= *MMUS_16(GPR_16(2));
	  break;
	case 0x20:
	  ifln(1) *MMUS_32(GPR_16(1)) |= GPR_32(2); 
	  else *MMUS_16(GPR_16(1)) |= GPR_16(2);
	  break;
	default:
	  ifln(1) GPR_32(1) |= GPR_32(2); 
	  else GPR_16(1) |= GPR_16(2);
	  break;
      }
      size = 3;
      break;
    case I_ORI:
      ifmem(1) *MMUS_16(GPR_16(1)) |= IMM_16(2);
      else GPR_16(1) |= IMM_16(2);
      size = 4;
      break;
    case I_AND:
      switch(REG_RM(insn[1])) {
	case 0x10:
	  ifln(1) GPR_32(1) &= *MMUS_32(GPR_16(2));
	  else GPR_16(1) &= *MMUS_16(GPR_16(2));
	  break;
	case 0x20:
	  ifln(1) *MMUS_32(GPR_16(1)) &= GPR_32(2); 
	  else *MMUS_16(GPR_16(1)) &= GPR_16(2);
	  break;
	default:
	  ifln(1) GPR_32(1) &= GPR_32(2); 
	  else GPR_16(1) &= GPR_16(2);
	  break;
      }
      size = 3;
      break;
    case I_ANDI:
      ifmem(1) *MMUS_16(GPR_16(1)) &= IMM_16(2);
      else GPR_16(1) &= IMM_16(2);
      size = 4;
      break;
    case I_NOT:
      switch(REG_LN(insn[1])) {
	case 0x40:
	  ifmem(1) *MMUS_32(GPR_16(1)) = ~(*MMUS_32(GPR_16(1)));
	  else GPR_32(1) = ~GPR_32(1);
	  break;
	default:
	  ifmem(1) *MMUS_16(GPR_16(1)) = ~(*MMUS_16(GPR_16(1)));
	  else GPR_16(1) = ~GPR_16(1);
	  break;
      }
      size = 2;
      break;

    case I_BTS:
      ifmem(1) *MMUS_32(GPR_16(1)) |= (1 << (insn[2] & 0x1F));
      else GPR_32(1) |= (1 << (insn[2] & 0x1F));
      size = 3;
      break;
    case I_BTR:
      ifmem(1) *MMUS_32(GPR_16(1)) &= ~(1 << (insn[2] & 0x1F));
      else GPR_32(1) &= ~(1 << (insn[2] & 0x1F));
      size = 3;
      break;
    case I_BTC:
      ifmem(1) *MMUS_32(GPR_16(1)) ^= (1 << (insn[2] & 0x1F));
      else GPR_32(1) ^= (1 << (insn[2] & 0x1F));
      size = 3;
      break;

    case I_SHL:
      switch(REG_LN(insn[1])) {
	case 0x40:
	  ifmem(1) *MMUS_32(GPR_16(1)) <<= (insn[2] & 0x1F);
	  else GPR_32(1) <<= (insn[2] & 0x1F);
	  break;
	default:
	  ifmem(1) *MMUS_16(GPR_16(1)) <<= (insn[2] & 0x0F);
	  else GPR_16(1) <<= (insn[2] & 0x0F);
	  break;
      }
      size = 3;
      break;
    case I_SHR:
      switch(REG_LN(insn[1])) {
	case 0x40:
	  ifmem(1) *MMUS_32(GPR_16(1)) >>= (insn[2] & 0x1F);
	  else GPR_32(1) >>= (insn[2] & 0x1F);
	  break;
	default:
	  ifmem(1) *MMUS_16(GPR_16(1)) >>= (insn[2] & 0x0F);
	  else GPR_16(1) >>= (insn[2] & 0x0F);
	  break;
      }
      size = 3;
      break;
    case I_ROL:
      switch(REG_LN(insn[1])) {
	case 0x40: 
	  { uint64_t inter;
	    ifmem(1) {
	      inter = ((uint64_t)*MMUS_32(GPR_16(1))) << (insn[2] & 0x1F);
	      *MMUS_32(GPR_16(1)) = inter | (inter >> 32);
	    } else {
	      inter = ((uint64_t)GPR_32(1)) << (insn[2] & 0x1F);
	      GPR_32(1) = inter | (inter >> 32);
	    };
	    break;
	  }
	default:
	  { uint32_t inter;
	    ifmem(1) {
	      inter = ((uint32_t)*MMUS_16(GPR_16(1))) << (insn[2] & 0xF);
	      *MMUS_16(GPR_16(1)) = inter | (inter >> 16);
	    } else {
	      inter = ((uint32_t)GPR_16(1)) << (insn[2] & 0xF);
	      GPR_16(1) = inter | (inter >> 16);
	    };
	    break;
	  }
      }
      size = 3;
      break;
    case I_ROR:
      switch(REG_LN(insn[1])) {
	case 0x40: 
	  { uint64_t inter;
	    ifmem(1) {
	      inter = ((uint64_t)*MMUS_32(GPR_16(1))) << (31 - (insn[2] & 0x1F));
	      *MMUS_32(GPR_16(1)) = inter | (inter >> 32);
	    } else {
	      inter = ((uint64_t)GPR_32(1)) << (31 - (insn[2] & 0x1F));
	      GPR_32(1) = inter | (inter >> 32);
	    };
	    break;
	  }
	default:
	  { uint32_t inter;
	    ifmem(1) {
	      inter = ((uint32_t)*MMUS_16(GPR_16(1))) << (15 - (insn[2] & 0xF));
	      *MMUS_16(GPR_16(1)) = inter | (inter >> 16);
	    } else {
	      inter = ((uint32_t)GPR_16(1)) << (15 - (insn[2] & 0xF));
	      GPR_16(1) = inter | (inter >> 16);
	    };
	    break;
	  }
      }
      size = 3;
      break;


    case I_INT:
      int_simple(s, core, insn[1]);
      core->sleep = 1;
      return;
    case I_JMP:
      GPR(IP) = IMM_16(1);
      core->sleep = 1;
      return;
    case I_CALL:
      *MMUS_16(GPR(SP) -= 2) = GPR(IP);
      GPR(IP) = IMM_16(1);
      core->sleep = 1;
      return;
    case I_IJMP:
      ifmem(1) GPR(IP) = *MMUS_16(GPR_16(1));
      else GPR(IP) = GPR_16(1);
      core->sleep = 1;
      return;
    case I_ICALL:
      *MMUS_16(GPR(SP) -= 2) = GPR(IP);
      ifmem(1) GPR(IP) = *MMUS_16(GPR_16(1));
      else GPR(IP) = GPR_16(1);
      core->sleep = 1;
      return;
    case I_RJMP:
      GPR(IP) += IMM_16(1);
      core->sleep = 1;
      return;
    case I_RCALL:
      *MMUS_16(GPR(SP) -= 2) = GPR(IP);
      GPR(IP) += IMM_16(1);
      core->sleep = 1;
      return;
    case I_RIJMP:
      ifmem(1) GPR(IP) += *MMUS_16(GPR_16(1));
      else GPR(IP) += GPR_16(1);
      core->sleep = 1;
      return;
    case I_RICALL:
      *MMUS_16(GPR(SP) -= 2) = GPR(IP);
      ifmem(1) GPR(IP) += *MMUS_16(GPR_16(1));
      else GPR(IP) += GPR_16(1);
      core->sleep = 1;
      return;
    case I_JC:
      {
	uint8_t op1 = insn[1] & 0x80 ? 1 : BIT_G(MSR(ALU), (insn[1] >> 4) & 0x7);
	uint8_t op2 = insn[1] & 0x08 ? 1 : BIT_G(MSR(ALU), insn[1] & 0x7);
	if (op1 == op2) { GPR(IP) = IMM_16(2); return; }
	core->sleep = 3;
	size = 4;
	break;
      }
    case I_JNC:
      {
	uint8_t op1 = insn[1] & 0x80 ? 1 : BIT_G(MSR(ALU), (insn[1] >> 4) & 0x7);
	uint8_t op2 = insn[1] & 0x08 ? 1 : BIT_G(MSR(ALU), insn[1] & 0x7);
	if (op1 != op2) { GPR(IP) = IMM_16(2); return; }
	core->sleep = 3;
	size = 4;
	break;
      }
    case I_MOVC:
      {
	uint8_t op1 = insn[1] & 0x80 ? 1 : BIT_G(MSR(ALU), (insn[1] >> 4) & 0x7);
	uint8_t op2 = insn[1] & 0x08 ? 1 : BIT_G(MSR(ALU), insn[1] & 0x7);
	core->sleep = 3;
	size = 4;
	if (op1 != op2) { break; }
	switch(REG_RM(insn[2])) {
	  case 0x10:
	    ifln(2) GPR_32(2) = *MMUS_32(GPR_16(3));
	    else GPR_16(2) = *MMUS_16(GPR_16(3));
	    break;
	  case 0x20:
	    ifln(2) *MMUS_32(GPR_16(2)) = GPR_32(3); 
	    else *MMUS_16(GPR_16(2)) = GPR_16(3);
	    break;
	  default:
	    ifln(2) GPR_32(2) = GPR_32(3); 
	    else GPR_16(2) = GPR_16(3);
	    break;
	}
	break;
      }
    case I_MOVNC:
      {
	uint8_t op1 = insn[1] & 0x80 ? 1 : BIT_G(MSR(ALU), (insn[1] >> 4) & 0x7);
	uint8_t op2 = insn[1] & 0x08 ? 1 : BIT_G(MSR(ALU), insn[1] & 0x7);
	core->sleep = 3;
	size = 4;
	if (op1 == op2) { break; }
	switch(REG_RM(insn[2])) {
	  case 0x10:
	    ifln(2) GPR_32(2) = *MMUS_32(GPR_16(3));
	    else GPR_16(2) = *MMUS_16(GPR_16(3));
	    break;
	  case 0x20:
	    ifln(2) *MMUS_32(GPR_16(2)) = GPR_32(3); 
	    else *MMUS_16(GPR_16(2)) = GPR_16(3);
	    break;
	  default:
	    ifln(2) GPR_32(2) = GPR_32(3); 
	    else GPR_16(2) = GPR_16(3);
	    break;
	}
	break;
      }
    case I_RET:
      GPR(IP) = *MMUS_16(GPR(SP)); 
      GPR(SP) += 2;
      return;

    case I_IN:
      switch(REG_LN(insn[1])) {
	case 0x40:
	  ifmem(1) *MMUS_32(GPR_16(1)) = *MMUI_32(GPR_16(2));
	  else GPR_32(1) = *MMUI_32(GPR_16(2));
	  break;
	default:
	  ifmem(1) *MMUS_16(GPR_16(1)) = *MMUI_16(GPR_16(2));
	  else GPR_16(1) = *MMUI_16(GPR_16(2));
	  break;
      }
      size = 3;
      break;
    case I_OUT:
      switch(REG_LN(insn[1])) {
	case 0x40:
	  ifmem(1) *MMUI_32(GPR_16(1)) = *MMUS_32(GPR_16(2));
	  else *MMUI_32(GPR_16(1)) = GPR_32(2);
	  break;
	default:
	  ifmem(1) *MMUI_16(GPR_16(1)) = *MMUS_16(GPR_16(2));
	  else *MMUI_16(GPR_16(1)) = GPR_16(2);
	  break;
      }
      mmu_iopc(s, core, GPR_16(2));
      size = 3;
      break;
    case I_INI:
      switch(REG_LN(insn[1])) {
	case 0x40:
	  ifmem(1) *MMUS_32(GPR_16(1)) = *MMUI_32(IMM_16(2));
	  else GPR_32(1) = *MMUI_32(IMM_16(2));
	  break;
	default:
	  ifmem(1) *MMUS_16(GPR_16(1)) = *MMUI_16(IMM_16(2));
	  else GPR_16(1) = *MMUI_16(IMM_16(2));
	  break;
      }
      size = 4;
      break;
    case I_OUTI:
      switch(REG_LN(insn[1])) {
	case 0x40:
	  ifmem(1) *MMUI_32(IMM_16(1)) = *MMUS_32(GPR_16(3));
	  else *MMUI_32(IMM_16(1)) = GPR_32(3);
	  break;
	default:
	  ifmem(1) *MMUI_16(IMM_16(1)) = *MMUS_16(GPR_16(3));
	  else *MMUI_16(IMM_16(1)) = GPR_16(3);
	  break;
      }
      mmu_iopc(s, core, GPR_16(2));
      size = 4;
      break;
    case I_CRLD:
      ifmem(1) *MMUS_32(GPR_16(1)) = MSR_32(2);
      else GPR_32(1) = MSR_32(2);
      size = 3;
      break;
    case I_CRST:
      ifmem(1) MSR_32(2) = *MMUS_32(GPR_16(1));
      else MSR_32(1) = GPR_32(2);
      core->status |= CPU_DIRTY;
      size = 3;
      break;

    case I_SEI:
      BIT_S(MSR(ALU), MSR_ALU_IF);
      size = 1;
      break;
    case I_CLI:
      BIT_R(MSR(ALU), MSR_ALU_IF);
      size = 1;
      break;
    case I_IRET:
      BIT_S(MSR(ALU), MSR_ALU_IF);
      GPR(IP) = *MMUS_16(GPR(SP)); 
      GPR(SP) += 2;
      return;


    case IP_FLAGS:
      switch (insn[1]) {
	case I_ADD:
	  switch (REG_LN(1)) {
	    case 0x40: 
	      { int32_t inter;
		switch(REG_RM(insn[1])) {
		  case 0x10:
		    inter = GPR_32(1) + *MMUS_32(GPR_16(2));
		    break;
		  case 0x20:
		    inter = *MMUS_32(GPR_16(1)) + GPR_32(2); 
		    break;
		  default:
		    inter = GPR_32(1) + GPR_32(2); 
		    break;
		}
		MSR(ALU) &= ~(MSR_ALU_FLAG_MASK);
		if (inter == 0) BIT_S(MSR(ALU), MSR_ALU_ZF);
		if (inter < 0) BIT_S(MSR(ALU), MSR_ALU_SF);
		if (GPR_16(1) & IMM_16(2) & 0x1000) BIT_S(MSR(ALU), MSR_ALU_CF);
		if (inter < 0 && GPR_16(1) & IMM_16(2) & 0x1000) BIT_S(MSR(ALU), MSR_ALU_OF);
		break;		
	      }
	    default:
	      { int16_t inter;
		switch(REG_RM(insn[1])) {
		  case 0x10:
		    inter = GPR_16(1) + *MMUS_16(GPR_16(2));
		    break;
		  case 0x20:
		    inter = *MMUS_16(GPR_16(1)) + GPR_16(2);
		    break;
		  default:
		    inter = GPR_16(1) + GPR_16(2);
		    break;
		}
		MSR(ALU) &= ~(MSR_ALU_FLAG_MASK);
		if (inter == 0) BIT_S(MSR(ALU), MSR_ALU_ZF);
		if (inter < 0) BIT_S(MSR(ALU), MSR_ALU_SF);
		if (GPR_16(1) & IMM_16(2) & 0x1000) BIT_S(MSR(ALU), MSR_ALU_CF);
		if (inter < 0 && GPR_16(1) & IMM_16(2) & 0x1000) BIT_S(MSR(ALU), MSR_ALU_OF);
		break;
	      }
	  }
	  size = 4;
	  break;
	case I_ADI:
	  { int16_t inter = GPR_16(1) + IMM_16(2);
	    MSR(ALU) &= ~(MSR_ALU_FLAG_MASK);
	    if (inter == 0) BIT_S(MSR(ALU), MSR_ALU_ZF);
	    if (inter < 0) BIT_S(MSR(ALU), MSR_ALU_SF);
	    if (GPR_16(1) & IMM_16(2) & 0x1000) BIT_S(MSR(ALU), MSR_ALU_CF);
	    if (inter < 0 && GPR_16(1) & IMM_16(2) & 0x1000) BIT_S(MSR(ALU), MSR_ALU_OF);
	    
	    size = 5;
	    break;
	  }
	case I_SUB:
	  switch (REG_LN(1)) {
	    case 0x40: 
	      { int32_t inter;
		switch(REG_RM(insn[1])) {
		  case 0x10:
		    inter = GPR_32(1) - *MMUS_32(GPR_16(2));
		    break;
		  case 0x20:
		    inter = *MMUS_32(GPR_16(1)) - GPR_32(2); 
		    break;
		  default:
		    inter = GPR_32(1) - GPR_32(2); 
		    break;
		}
		MSR(ALU) &= ~(MSR_ALU_FLAG_MASK);
		if (inter == 0) BIT_S(MSR(ALU), MSR_ALU_ZF);
		if (inter < 0) BIT_S(MSR(ALU), MSR_ALU_SF);
		if (inter < 0) BIT_S(MSR(ALU), MSR_ALU_CF);
		break;		
	      }
	    default:
	      { int16_t inter;
		switch(REG_RM(insn[1])) {
		  case 0x10:
		    inter = GPR_16(1) - *MMUS_16(GPR_16(2));
		    break;
		  case 0x20:
		    inter = *MMUS_16(GPR_16(1)) - GPR_16(2);
		    break;
		  default:
		    inter = GPR_16(1) - GPR_16(2);
		    break;
		}
		MSR(ALU) &= ~(MSR_ALU_FLAG_MASK);
		if (inter == 0) BIT_S(MSR(ALU), MSR_ALU_ZF);
		if (inter < 0) BIT_S(MSR(ALU), MSR_ALU_SF);
		if (inter < 0) BIT_S(MSR(ALU), MSR_ALU_CF);
		break;
	      }
	  }
	  size = 4;
	  break;
	case I_SBI:
	  { int16_t inter = GPR_16(1) - IMM_16(2);
	    MSR(ALU) &= ~(MSR_ALU_FLAG_MASK);
	    if (inter == 0) BIT_S(MSR(ALU), MSR_ALU_ZF);
	    if (inter < 0) BIT_S(MSR(ALU), MSR_ALU_SF);
	    if (inter < 0) BIT_S(MSR(ALU), MSR_ALU_CF);
	    size = 5;
	    break;
	  }

	default:
	  int_simple(s, core, INT_IL);
	  return;
      }
      sleep = 1;
      break;

    default:
      int_simple(s, core, INT_IL);
      return;
  }
  core->sleep = sleep;
  GPR(IP) += size;
}

void execute_complex(hw *s, cpu *core) {
  uint8_t *insn = MMUC_8(GPR(IP), MMU_EXEC);
  if (!insn) return;
  if (((insn[0] & 0xC0) == 0xC0) && !BIT_G(MSR(MODE), MSR_MODE_KERNEL)) {
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
    case I_PUSH:
      size = 2;
      break;
    case I_POP:
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
    case I_JC:
      break;
    case I_JNC:
      break;
    case I_RET:
      size = 1;
      break;


    case I_IN:
      size = 4;
      break;
    case I_OUT:
      size = 4;
      break;
    case I_INI:
      size = 3;
      break;
    case I_OUTI:
      size = 3;
      break;
    case I_CRLD:
      size = 3;
      break;
    case I_CRST:
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
    if (core->status & CPU_RUNNING) {
      core->ip_count++;
      if (core->status & CPU_TIMER1) {
	MSR(TIMERC1)--;
	if (MSR(TIMERC1) == 0) { 
	  if (core->status & CPU_COMPLEX) int_complex(s, core, INT_TIMER1);
	  else int_simple(s, core, INT_TIMER1);
	  MSR(TIMERC1) = MASK_G(MSR(TIMER1), MSR_TIMER_VALUE) << MASK_G(MSR(TIMER1), MSR_TIMER_MULT);
	}
      }
      if (core->status & CPU_TIMER2) {
	MSR(TIMERC2)--;	
	if (MSR(TIMERC2) == 0) { 
	  if (core->status & CPU_COMPLEX) int_complex(s, core, INT_TIMER2);
	  else int_simple(s, core, INT_TIMER2);
	  MSR(TIMERC2) = MASK_G(MSR(TIMER2), MSR_TIMER_VALUE) << MASK_G(MSR(TIMER2), MSR_TIMER_MULT);
	}
      }
      if (core->sleep > 0) {
	core->sleep--;
	if (core->sleep == 0) core->status &= ~CPU_ASLEEP;
	continue;
      }
      if (core->status & CPU_COMPLEX) execute_complex(s, core);
      else execute_simple(s, core);
      if (core->status & CPU_DIRTY) {
	core->status &= ~CPU_DIRTY;
	//TODO update CPU props
      }
    }
  } 
}

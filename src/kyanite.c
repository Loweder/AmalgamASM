#include <aasm/kyanite/instruction.h>
#include <aasm/kyanite/interrupt.h>

#include <string.h>

#define WIDTH_MASK(val, width) (val & ((1 << width) - 1))

#define ST(name) (STATUS_KYANITE_ ## name)
#define ST_SET(dev, name) (dev).status |= ST(name);
#define ST_UNSET(dev, name) (dev).status &= ~ST(name);
#define ST_TEST(dev, name) (ST(name) & (dev).status)

#define BUS_RESP(dev, core, type, value) \
  if (ST_TEST(*dev, IO_OUTPUT)) return; \
  ST_SET(*dev, IO_ ## type); dev->bus_out = value; dev->bus_ocore = core;
#define BUS_REQ(dev, core, type, address, value) \
  if (ST_TEST(*dev, IO_INPUT)) return; \
  ST_SET(*dev, IO_ ## type); dev->bus_in = value; dev->bus_icore = core; dev->bus_address = address;
#define BUS_RESPM(dev, core, type, value) \
  if (ST_TEST(*dev, IO_OUTPUT)) return; \
  dev->status |= type; dev->bus_out = value; dev->bus_ocore = core;
#define BUS_REQM(dev, core, type, address, value) \
  if (ST_TEST(*dev, IO_INPUT)) return; \
  dev->status |= type; dev->bus_in = value; dev->bus_icore = core; dev->bus_address = address;

/*
#define IS_COMPLEX (core->basic.status & STATUS_KYANITE_CPU_COMPLEX)

#define ASSERT_SPEC(task, condition) if (condition) { task }
#define INTERRUPT(type) { if (!(task & MMU_SYSRD)) interrupt(s, core, INT_ ## type); return 1; }

uint8_t mmu_io(kyanite_board_t *s, kyanite_cpu_t *core, uint16_t addr, uint8_t size, uint64_t *data, mmu_req_t task) {
  kyanite_io_t *io = (kyanite_io_t*) s->basic.devices[KYANITE_IO]->data;
  uint8_t segment = addr >> 11;
  ASSERT_INT(IO, segment > io->device_count);
  ASSERT_INT(IO, !(io->devices[segment]->basic.data->status & STATUS_KYANITE_IO_PRESENT));
  ASSERT_INT(IO, !io->devices[segment]->processor(io->devices[segment]->basic.data, addr & 0x1FFF, size, data, task & 0x1));
  return 0;

}
uint8_t mmu_ram(kyanite_board_t *s, kyanite_cpu_t *core, uint32_t addr, uint8_t size, uint64_t *data, mmu_req_t task) {
  kyanite_io_t *ram = (kyanite_io_t*) s->basic.devices[KYANITE_BOARD_RAM];
  if (IS_COMPLEX) {
    mmu_addr_t adv_addr = *(mmu_addr_t*) &addr;
    uint32_t paging = KYANITE_MSR(PAGING);

    ASSERT_INT(MR, s->ram_size < ((uint64_t)MMU_PG_TABLE_MASK(paging) + 0x1000));
    mmu_page_t page_dir = ((mmu_page_t*)s->ram + MMU_PG_TABLE_MASK(paging))[adv_addr.filed.dir];
    ASSERT_SPEC(KYANITE_MSR(PAGE_FAULT) = addr; INTERRUPT(PF);, !page_dir.available);
    ASSERT_INT(GP, (task & page_dir.mode & 3) != (task & 3));
    ASSERT_INT(GP, page_dir.ring && !(KYANITE_MSR(MODE) & KYANITE_MSR_MODE_KERNEL));

    ASSERT_INT(MR, s->ram_size < ((uint64_t)MMU_PG_DIR_MASK(page_dir) + 0x1000));
    mmu_page_t page_entry = page_dir.type ? page_dir : ((mmu_page_t*)s->ram + MMU_PG_DIR_MASK(page_dir))[adv_addr.filed.file];
    ASSERT_SPEC(KYANITE_MSR(PAGE_FAULT) = addr; INTERRUPT(PF);, !page_entry.available);
    ASSERT_INT(GP, (task & page_entry.mode & 3) != (task & 3));
    ASSERT_INT(GP, page_entry.ring && !(KYANITE_MSR(MODE) & KYANITE_MSR_MODE_KERNEL));

    int32_t over = page_dir.type ? 
      (adv_addr.supreme.offset + size) - 0x400000 : 
      (adv_addr.filed.offset + size) - 0x1000;
    if (over > 0) {
      size -= over;
      ASSERT_PASS(mmu_ram(s, core, addr + size, over, (uint64_t*)(((uint8_t*)data) + over), task));
    }
    addr = page_dir.type ?
      MMU_PG_SUPREME_MASK(page_entry) + adv_addr.supreme.offset :
      MMU_PG_FILE_MASK(page_entry) + adv_addr.filed.offset;
    if (page_entry.device) {
      device_t *io = s->basic.devices[KYANITE_BOARD_IO];
      ASSERT_INT(IO, page_entry.device > io->device_count);
      ASSERT_INT(IO, !(io->devices[page_entry.device]->status & STATUS_KYANITE_IO_PRESENT));
      ASSERT_INT(IO, ((kyanite_io_t*)io->devices[page_entry.device])->processor(io->devices[page_entry.device], addr, size, data, task & 0x1));
      return 0;
    }
  } else {
    addr &= 0x0000FFFF;
  }
  ASSERT_INT(MR, s->ram_size < addr + size || (s->ram_size <= addr && (task & MMU_EXEC)));
  if (task & MMU_WRITE) {
    memcpy(s->ram + addr, data, size);
  } else {
    *data = 0;
    if (task & MMU_EXEC)
      memcpy(data, s->ram + addr, s->ram_size < addr + size ? s->ram_size - addr : size);
    else
      memcpy(data, s->ram + addr, size);
  }
  return 0;
}
#undef INTERRUPT
#define INTERRUPT(type) { interrupt(s, core, INT_ ## type); return 1; }

void interrupt(kyanite_board_t *s, kyanite_cpu_t *core, uint8_t number) {
  if (!(KYANITE_MSR(ALU) & KYANITE_MSR_ALU_IF) && number > INT_LAST_NMI) return;
  if (core->basic.status & STATUS_KYANITE_CPU_ASLEEP) { //Wake up from sleep
    core->basic.status &= ~STATUS_KYANITE_CPU_ASLEEP;
    core->delay = 0;
  }
  uint32_t table_base = number*2;
  uint8_t size = 2;
  if (IS_COMPLEX) {
    table_base = KYANITE_MSR(INT) + number*4;
    size = 4; 
    //Switch to kernel ring and store old access
    if (KYANITE_MSR(MODE) & KYANITE_MSR_MODE_KERNEL) KYANITE_MSR(INT) |= KYANITE_MSR_INT_KERNEL;
    else KYANITE_MSR(INT) &= ~KYANITE_MSR_INT_KERNEL;
    KYANITE_MSR(MODE) |= KYANITE_MSR_MODE_KERNEL;
  }
  uint64_t addr = 0;
  if (mmu_ram(s, core, table_base, size, &addr, MMU_SYSRD) || 
      mmu_ram(s, core, KYANITE_GPR(SP) -= size, size, &KYANITE_GPR(IP), MMU_SYSWR)) { //Shutdown
    core->basic.status |= STATUS_KYANITE_CPU_INT_FAIL;
    core->basic.status &= ~STATUS_RUNNING;
    return;
  }
  KYANITE_GPR(IP) = addr; //Set IP
  return;
}

inline void insn_read_reg(kyanite_cpu_t *core, uint8_t reg, uint64_t *to, uint8_t size) {
  *to = core->gpr[reg] & (size == 8 ? 0xFFFFFFFFFFFFFFFF : (size == 4 ? 0xFFFFFFFF : 0xFFFF));
}
inline void insn_write_reg(kyanite_cpu_t *core, uint8_t reg, uint64_t *from, uint8_t size) {
  core->gpr[reg] = *from & (size == 8 ? 0xFFFFFFFFFFFFFFFF : (size == 4 ? 0xFFFFFFFF : 0xFFFF));
}
inline uint8_t insn_decode_address(kyanite_cpu_t *core, insn_mem_t address, uint32_t *to) {
  switch (address.mode) {
    case INSN_MEM_REG:
      return 1;
    case INSN_MEM_PURE:
      *to = core->gpr[address.reg1];
      break;
    case INSN_MEM_LOCAL:
      *to = KYANITE_GPR(IP);
      goto offcalc;
    case INSN_MEM_IMPURE:
      *to = core->gpr[address.reg1];
offcalc:
      if (address.offset_type) {
	*to += core->gpr[address.offset.scaled.reg2] * address.offset.scaled.scalar;
      } else *to += address.offset.direct;
      break;
  }
  return 0;
}
uint8_t insn_read_address(kyanite_board_t *s, kyanite_cpu_t *core, insn_mem_t address, uint64_t *to, uint8_t size) {
  uint32_t offset = 0;
  if (insn_decode_address(core, address, &offset)) {
    insn_read_reg(core, address.reg1, to, size);
    return 0;
  }
  return mmu_ram(s, core, offset, size, to, MMU_READ);
}
uint8_t insn_write_address(kyanite_board_t *s, kyanite_cpu_t *core, insn_mem_t address, uint64_t *from, uint8_t size) {
  uint32_t offset = 0;
  if (insn_decode_address(core, address, &offset)) {
    insn_write_reg(core, address.reg1, from, size);
    return 0;
  }
  return mmu_ram(s, core, offset, size, from, MMU_WRITE);
}

#define INSN_MEM_S insn_mem_t mem_multi = *(insn_mem_t*)(&insn + 2);
#define INSN_IMM_S uint64_t src = (*(uint32_t*)(&insn + 2)) & (size == 4 ? 0xFFFFFFFF : 0xFFFF); //Replaces INSN_GET_UNI/MUNI
#define INSN_MEM_U insn_mem_t mem_uni = *(insn_mem_t*)(&insn + 1);
#define INSN_IMM_U uint64_t dest = (*(uint32_t*)(&insn + 1)) & (size == 4 ? 0xFFFFFFFF : 0xFFFF); //Replaces INSN_GET_UNI/MUNI

#define INSN_GET_MSRC \
  uint64_t src = 0; \
  if (insn.mem.direction) insn_read_reg(core, insn.mem.reg1, &src, size << insn.mem.extended); \
  else ASSERT_VOID(insn_read_address(s, core, mem_multi, &src, size << insn.mem.extended));
#define INSN_GET_MDEST \
  uint64_t dest = 0; \
  if (!insn.mem.direction) insn_read_reg(core, insn.mem.reg1, &dest, size << insn.mem.extended); \
  else ASSERT_VOID(insn_read_address(s, core, mem_multi, &dest, size << insn.mem.extended));
#define INSN_SET_MDEST \
  if (!insn.mem.direction) insn_write_reg(core, insn.mem.reg1, &dest, size << insn.mem.extended); \
  else ASSERT_VOID(insn_write_address(s, core, mem_multi, &dest, size << insn.mem.extended));

#define INSN_GET_SRC \
  uint64_t src = 0; \
  insn_read_reg(core, insn.reg.reg2, &src, size << insn.reg.extended);
#define INSN_GET_DEST \
  uint64_t dest = 0; \
  insn_read_reg(core, insn.reg.reg1, &dest, size << insn.reg.extended);
#define INSN_SET_DEST \
  insn_write_reg(core, insn.reg.reg1, &dest, size << insn.reg.extended);

#define INSN_GET_MUNI \
  uint64_t dest = 0; \
  ASSERT_VOID(insn_read_address(s, core, mem_uni, &dest, size << insn.mem.extended));
#define INSN_SET_MUNI \
  ASSERT_VOID(insn_write_address(s, core, mem_uni, &dest, size << insn.mem.extended));

void kyanite_cpu_executor(kyanite_board_t *s, kyanite_cpu_t *core) {
  insn_t insn = {0};
    if (mmu_ram(s, core, KYANITE_GPR(IP), 8, (uint64_t*) &insn, MMU_EXEC)) return;
    ASSERT_SPEC(interrupt(s, core, INT_GP); return;, IS_COMPLEX && ((insn.pure.opcode & 0xC0) == 0xC0) && !(KYANITE_MSR(MODE) & KYANITE_MSR_MODE_KERNEL));
    const uint8_t size = IS_COMPLEX ? 4 : 2;
    const uint8_t bit_mask = IS_COMPLEX ? 0x3F : 0x1F;
  //TODO add sizes
  switch (insn.pure.opcode) {
  case I_NOP:
  break;
  case I_SLEEP:
  {
  INSN_MEM_U;
  INSN_GET_MUNI;
  core->delay += dest;
  core->basic.status |= STATUS_KYANITE_CPU_ASLEEP;
  break;
  }
  case I_SLEEPI:
  {
  INSN_IMM_U;
  core->delay += dest;
  core->basic.status |= STATUS_KYANITE_CPU_ASLEEP;
  break;
  }
  case I_GIPC:
  {
  INSN_MEM_U;
  uint64_t dest = core->ip_count;
  INSN_SET_MUNI;
  break;
  }

  case I_MOV:
  {
  INSN_MEM_S;
  INSN_GET_MSRC;
  INSN_GET_MDEST;
  dest = src;
  INSN_SET_MDEST;
  break;
  }
  case I_MOVI:
  {
  INSN_IMM_S;
  uint64_t dest = src;
  INSN_SET_DEST;
  break;
  }
  case I_SWAP:
  { //Requires special handling
  INSN_GET_SRC;
  INSN_GET_DEST;
  core->gpr[insn.reg.reg1] = src;
  core->gpr[insn.reg.reg2] = dest;
  break;
  }
  case I_PUSH:
  {
  INSN_GET_DEST;
  ASSERT_VOID(mmu_ram(s, core, KYANITE_GPR(SP) -= size, size, &dest, MMU_WRITE));
  break;
  }
  case I_POP:
  {
  uint64_t dest = 0;
  ASSERT_VOID(mmu_ram(s, core, KYANITE_GPR(SP), size, &dest, MMU_READ));
  INSN_SET_DEST;
  KYANITE_GPR(SP) += size;
  break;
  }

  case I_ADD:
    {
      INSN_MEM_S;
      INSN_GET_MSRC;
      INSN_GET_MDEST;
      dest += src;
      INSN_SET_MDEST;
      break;
    }
  case I_ADI:
    {
      INSN_IMM_S;
      INSN_GET_DEST;
      dest += src;
      INSN_SET_DEST;
      break;
    }
  case I_SUB:
    {
      INSN_MEM_S;
      INSN_GET_MSRC;
      INSN_GET_MDEST;
      dest -= src;
      INSN_SET_MDEST;
      break;
    }
  case I_SBI:
    {
      INSN_IMM_S;
      INSN_GET_DEST;
      dest -= src;
      INSN_SET_DEST;
      break;
    }

  case I_MUL:
    {
      INSN_MEM_S;
      INSN_GET_MSRC;
      INSN_GET_MDEST;
      dest *= src;
      INSN_SET_MDEST;
      break;
    }
  case I_DIV:
    {
      INSN_MEM_S;
      INSN_GET_MSRC;
      INSN_GET_MDEST;
      ASSERT_SPEC(interrupt(s, core, INT_DV); return;, !src);
      dest /= src;
      INSN_SET_MDEST;
      break;
    }
  case I_MOD:
    {
      INSN_MEM_S;
      INSN_GET_MSRC;
      INSN_GET_MDEST;
      ASSERT_SPEC(interrupt(s, core, INT_DV); return;, !src);
      dest %= src;
      INSN_SET_MDEST;
      break;
    }
  case I_INC:
    {
      INSN_MEM_U;
      INSN_GET_MUNI;
      dest++;
      INSN_SET_MUNI;
      break;
    }
  case I_DEC:
    {
      INSN_MEM_U;
      INSN_GET_MUNI;
      dest--;
      INSN_SET_MUNI;
      break;
    }

  case I_XOR:
    {
      INSN_MEM_S;
      INSN_GET_MSRC;
      INSN_GET_MDEST;
      dest ^= src;
      INSN_SET_MDEST;
      break;
    }
  case I_XORI:
    {
      INSN_IMM_S;
      INSN_GET_DEST;
      dest ^= src;
      INSN_SET_DEST;
      break;
    }
  case I_OR:
    {
      INSN_MEM_S;
      INSN_GET_MSRC;
      INSN_GET_MDEST;
      dest |= src;
      INSN_SET_MDEST;
      break;
    }
  case I_ORI:
    {
      INSN_IMM_S;
      INSN_GET_DEST;
      dest |= src;
      INSN_SET_DEST;
      break;
    }
  case I_AND:
    {
      INSN_MEM_S;
      INSN_GET_MSRC;
      INSN_GET_MDEST;
      dest &= src;
      INSN_SET_MDEST;
      break;
    }
  case I_ANDI:
    {
      INSN_IMM_S;
      INSN_GET_DEST;
      dest &= src;
      INSN_SET_DEST;
      break;
    }
  case I_NOT:
    {
      INSN_MEM_U;
      INSN_GET_MUNI;
      dest = ~dest;
      INSN_SET_MUNI;
      break;
    }

  case I_BTS:
    {
      INSN_MEM_S;
      INSN_GET_MSRC;
      INSN_GET_MDEST;
      dest |= (1 << (src & bit_mask));
      INSN_SET_MDEST;
      break;
    }
  case I_BTSI:
    {
      INSN_IMM_S;
      INSN_GET_DEST;
      dest |= (1 << (src & bit_mask));
      INSN_SET_DEST;
      break;
    }
  case I_BTR:
    {
      INSN_MEM_S;
      INSN_GET_MSRC;
      INSN_GET_MDEST;
      dest &= ~(1 << (src & bit_mask));
      INSN_SET_MDEST;
      break;
    }
  case I_BTRI:
    {
      INSN_IMM_S;
      INSN_GET_DEST;
      dest &= ~(1 << (src & bit_mask));
      INSN_SET_DEST;
      break;
    }
  case I_BTC:
    {
      INSN_MEM_S;
      INSN_GET_MSRC;
      INSN_GET_MDEST;
      dest ^= (1 << (src & bit_mask));
      INSN_SET_MDEST;
      break;
    }
  case I_BTCI:
    {
      INSN_IMM_S;
      INSN_GET_DEST;
      dest ^= (1 << (src & bit_mask));
      INSN_SET_DEST;
      break;
    }

  case I_SHL:
    {
      INSN_MEM_S;
      INSN_GET_MSRC;
      INSN_GET_MDEST;
      dest <<= (src & bit_mask);
      INSN_SET_MDEST;
      break;
    }
  case I_SHLI:
    {
      INSN_IMM_S;
      INSN_GET_DEST;
      dest <<= (src & bit_mask);
      INSN_SET_DEST;
      break;
    }
  case I_SHR:
    {
      INSN_MEM_S;
      INSN_GET_MSRC;
      INSN_GET_MDEST;
      dest >>= (src & bit_mask);
      INSN_SET_MDEST;
      break;
    }
  case I_SHRI:
    {
      INSN_IMM_S;
      INSN_GET_DEST;
      dest >>= (src & bit_mask);
      INSN_SET_DEST;
      break;
    }
  case I_ROL:
    {
      INSN_MEM_S;
      INSN_GET_MSRC;
      INSN_GET_MDEST;
      dest = (dest >> ((size << 4) - (src & bit_mask))) | (dest << (src & bit_mask));
      INSN_SET_MDEST;
      break;
    }
  case I_ROLI:
    {
      INSN_IMM_S;
      INSN_GET_DEST;
      dest = (dest >> ((size << 4) - (src & bit_mask))) | (dest << (src & bit_mask));
      INSN_SET_DEST;
      break;
    }
  case I_ROR:
    {
      INSN_MEM_S;
      INSN_GET_MSRC;
      INSN_GET_MDEST;
      dest = (dest << ((size << 4) - (src & bit_mask))) | (dest >> (src & bit_mask));
      INSN_SET_MDEST;
      break;
    }
  case I_RORI:
    {
      INSN_IMM_S;
      INSN_GET_DEST;
      dest = (dest << ((size << 4) - (src & bit_mask))) | (dest >> (src & bit_mask));
      INSN_SET_DEST;
      break;
    }

  case I_INT: 
    {
      KYANITE_GPR(IP) += size + 1;
      INSN_IMM_U;
      interrupt(s, core, dest);
      return;
    }
  case I_JMP:
    {
      INSN_MEM_U;
      uint32_t address = 0;
      ASSERT_SPEC(interrupt(s, core, INT_UD); return;, insn_decode_address(core, mem_uni, &address));
      KYANITE_GPR(IP) = address;
      return;
    }
  case I_CALL:
    {
      INSN_MEM_U;
      uint32_t address = 0;
      ASSERT_SPEC(interrupt(s, core, INT_UD); return;, insn_decode_address(core, mem_uni, &address));
      ASSERT_VOID(mmu_ram(s, core, KYANITE_GPR(SP) -= size, size, &KYANITE_GPR(IP), MMU_WRITE));
      KYANITE_GPR(IP) = address;
      return;
    }
  case I_JC:
    {
      //TODO
      uint8_t op1 = 0;//insn[1] & 0x80 ? 1 : BIT_G(MSR(ALU), (insn[1] >> 4) & 0x7);
      uint8_t op2 = 0;//insn[1] & 0x08 ? 1 : BIT_G(MSR(ALU), insn[1] & 0x7);
      if (op1 != op2) break;
      INSN_MEM_U;
      uint32_t address = 0;
      ASSERT_SPEC(interrupt(s, core, INT_UD); return;, insn_decode_address(core, mem_uni, &address));
      KYANITE_GPR(IP) = address;
      return;
    }
  case I_JNC:
    {
      uint8_t op1 = 0;//insn[1] & 0x80 ? 1 : BIT_G(MSR(ALU), (insn[1] >> 4) & 0x7);
      uint8_t op2 = 0;//insn[1] & 0x08 ? 1 : BIT_G(MSR(ALU), insn[1] & 0x7);
      if (op1 == op2) break;
      INSN_MEM_U;
      uint32_t address = 0;
      ASSERT_SPEC(interrupt(s, core, INT_UD); return;, insn_decode_address(core, mem_uni, &address));
      KYANITE_GPR(IP) = address;
      return;
    }
  case I_MOVC:
    {
      uint8_t op1 = 0;//insn[1] & 0x80 ? 1 : BIT_G(MSR(ALU), (insn[1] >> 4) & 0x7);
      uint8_t op2 = 0;//insn[1] & 0x08 ? 1 : BIT_G(MSR(ALU), insn[1] & 0x7);
      if (op1 != op2) break;
      INSN_MEM_S;
      INSN_GET_MSRC;
      INSN_GET_MDEST;
      dest = src;
      INSN_SET_MDEST;
      break;
    }
  case I_MOVNC:
    {
      uint8_t op1 = 0;//insn[1] & 0x80 ? 1 : BIT_G(MSR(ALU), (insn[1] >> 4) & 0x7);
      uint8_t op2 = 0;//insn[1] & 0x08 ? 1 : BIT_G(MSR(ALU), insn[1] & 0x7);
      if (op1 == op2) break;
      INSN_MEM_S;
      INSN_GET_MSRC;
      INSN_GET_MDEST;
      dest = src;
      INSN_SET_MDEST;
      break;
    }
  case I_RET:
    {
      ASSERT_VOID(mmu_ram(s, core, KYANITE_GPR(SP), size, &KYANITE_GPR(IP), MMU_READ));
      KYANITE_GPR(SP) += size;
      return;
    }

  case I_IN:
    {
      INSN_MEM_S;
      INSN_GET_MSRC;
      uint64_t dest = 0;
      mmu_io(s, core, src, size << insn.mem.extended, &dest, MMU_READ);
      INSN_SET_MDEST;
      break;
    }
  case I_INI:
    {
      INSN_IMM_S;
      uint64_t dest = 0;
      mmu_io(s, core, src, size << insn.reg.extended, &dest, MMU_READ);
      INSN_SET_DEST;
      break;
    }
  case I_OUT:
    {
      INSN_MEM_S;
      INSN_GET_MSRC;
      INSN_GET_MDEST;
      mmu_io(s, core, dest, size << insn.mem.extended, &src, MMU_WRITE);
      break;
    }
  case I_OUTI:
    {
      INSN_IMM_S;
      INSN_GET_DEST;
      mmu_io(s, core, dest, size << insn.reg.extended, &src, MMU_WRITE);
      break;
    }
  case I_CRLD:
    {
      uint64_t dest = core->msr[insn.reg.reg2];
      INSN_SET_DEST;
      break;
    }
  case I_CRST:
    {
      INSN_GET_SRC;
      core->msr[insn.reg.reg1] = src;
      break;
    }

  case I_SEI:
    {
      KYANITE_MSR(ALU) |= KYANITE_MSR_ALU_IF;
      break;
    }
  case I_CLI:
    {
      KYANITE_MSR(ALU) &= !KYANITE_MSR_ALU_IF;
      break;
    }
  case I_IRET:
    {
      ASSERT_VOID(mmu_ram(s, core, KYANITE_GPR(SP), size, &KYANITE_GPR(IP), MMU_READ));
      KYANITE_GPR(SP) += size;
      if (IS_COMPLEX) {
	if (KYANITE_MSR(INT) & KYANITE_MSR_INT_KERNEL) KYANITE_MSR(MODE) |= KYANITE_MSR_MODE_KERNEL;
	else KYANITE_MSR(MODE) &= ~KYANITE_MSR_MODE_KERNEL;
      }
      return;
    }
  default:
    interrupt(s, core, INT_UD);
    return;
}
}*/

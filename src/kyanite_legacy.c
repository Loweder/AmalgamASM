#include <aasm/kyanite/legacy.h>
#include <aasm/kyanite/instruction.h>
#include <aasm/kyanite/interrupt.h>

#include <string.h>

#define ST(name) (STATUS_KYANITE_ ## name)
#define ST_SET(dev, name) (dev).status |= ST(name);
#define ST_UNSET(dev, name) (dev).status &= ~ST(name);
#define ST_TEST(dev, name) (ST(name) & (dev).status)

#define DYN_MASK(size) ((1 << (size)) - 1)

//Hardware microtasks
#define BUS_RESP(dev, type, value) {\
  if (ST_TEST(*dev, IO_OUTPUT)) return; \
  ST_SET(*dev, IO_ ## type); dev->bus_out = value; \
}
#define BUS_REQ(dev, type, address, value) {\
  if (ST_TEST(*dev, IO_INPUT)) return; \
  ST_SET(*dev, IO_ ## type); dev->bus_in = value; dev->bus_address = address; \
}
#define BUS_RESPM(dev, type, value) {\
  if (ST_TEST(*dev, IO_OUTPUT)) return; \
  dev->status |= type; dev->bus_out = value; \
}
#define BUS_REQM(dev, type, address, value) {\
  if (ST_TEST(*dev, IO_INPUT)) return; \
  dev->status |= type; dev->bus_in = value; dev->bus_address = address; \
}

//CPU microtasks
#define INSN_END(size) core->ip.raw += size; break
#define INSN_PURE(opcode, code) { \
  case opcode: \
    code; \
    INSN_END(1); \
}
#define INSN_ALU(opcode, code) { \
  { \
    uint16_t dest, src; \
    _legacy_addr_t mem_op; \
    case opcode ## rr: \
      src = core->gpr[insn.mem.mode]; \
      dest = core->gpr[insn.mem.reg]; \
      code; \
      core->gpr[insn.opimm.reg] = dest; \
      INSN_END(2); \
    case opcode ## mr: \
      mem_op = parse_mem(core, insn); \
      CACHE_CHECK(core, mem_op, 2); \
      src = core->gpr[insn.mem.reg]; \
      CACHE_READ(core, mem_op, 2, dest); \
      code; \
      CACHE_WRITE(core, mem_op, 2, dest); \
      INSN_END(2); \
    case opcode ## rm: \
      mem_op = parse_mem(core, insn); \
      CACHE_CHECK(core, mem_op, 2); \
      CACHE_READ(core, mem_op, 2, src); \
      dest = core->gpr[insn.mem.reg]; \
      code; \
      core->gpr[insn.mem.reg] = dest; \
      INSN_END(2); \
  } \
  { \
    uint32_t dest, src; \
    _legacy_addr_t mem_op; \
    case opcode ## xx: \
      src = core->gpr[insn.mem.mode]; \
      dest = core->gpr[insn.mem.reg]; \
      code; \
      core->gpr[insn.opimm.reg] = dest; \
      INSN_END(2); \
    case opcode ## lx: \
      mem_op = parse_mem(core, insn); \
      CACHE_CHECK(core, mem_op, 4); \
      src = core->gpr[insn.mem.reg]; \
      CACHE_READ(core, mem_op, 4, dest); \
      code; \
      CACHE_WRITE(core, mem_op, 4, dest); \
      INSN_END(2); \
    case opcode ## xl: \
      mem_op = parse_mem(core, insn); \
      CACHE_CHECK(core, mem_op, 4); \
      CACHE_READ(core, mem_op, 4, src); \
      dest = core->gpr[insn.mem.reg]; \
      code; \
      core->gpr[insn.mem.reg] = dest; \
      INSN_END(2); \
  } \
}
#define INSN_ALU_IMM(opcode, code) { \
  { \
    uint16_t dest, src; \
    case opcode ## ri: \
      src = insn.opimm.imm; \
      dest = core->gpr[insn.opimm.reg]; \
      code; \
      core->gpr[insn.opimm.reg] = dest; \
      INSN_END(4); \
  } \
  { \
    uint32_t dest, src; \
    case opcode ## xi: \
      src = insn.opimm.imm; \
      dest = core->gpr[insn.opimm.reg]; \
      code; \
      core->gpr[insn.opimm.reg] = dest; \
      INSN_END(4); \
  } \
}
#define INSN_BIT(opcode, code) { \
  uint8_t index; \
  { \
    uint16_t reg; \
    case opcode ## rr: \
      index = core->gpr[insn.mem.mode]; \
      reg = core->gpr[insn.mem.reg]; \
      code; \
      core->gpr[insn.mem.reg] = reg; \
      INSN_END(2); \
  } \
  { \
    uint32_t reg; \
    case opcode ## xr: \
      index = core->gpr[insn.mem.mode]; \
      reg = core->gpr[insn.mem.reg]; \
      code; \
      core->gpr[insn.mem.reg] = reg; \
      INSN_END(2); \
  } \
}
#define INSN_READ(opcode, code) { \
  _legacy_addr_t mem_op; \
  { \
    uint16_t reg; \
    case opcode ## r: \
      reg = core->gpr[insn.mem.reg]; \
      code; \
      INSN_END(2); \
    case opcode ## m: \
      mem_op = parse_mem(core, insn); \
      CACHE_CHECK(core, mem_op, 2); \
      CACHE_READ(core, mem_op, 2, reg); \
      code; \
      INSN_END(2); \
  } \
  { \
    uint32_t reg; \
    case opcode ## x: \
      reg = core->gpr[insn.mem.reg]; \
      code; \
      INSN_END(2); \
    case opcode ## l: \
      mem_op = parse_mem(core, insn); \
      CACHE_CHECK(core, mem_op, 4); \
      CACHE_READ(core, mem_op, 4, reg); \
      code; \
      INSN_END(2); \
  } \
}
#define INSN_WRITE(opcode, code) { \
  _legacy_addr_t mem_op; \
  { \
    uint16_t reg; \
    case opcode ## r: \
      code; \
      core->gpr[insn.mem.reg] = reg; \
      INSN_END(2); \
    case opcode ## m: \
      mem_op = parse_mem(core, insn); \
      CACHE_CHECK(core, mem_op, 2); \
      code; \
      CACHE_WRITE(core, mem_op, 2, reg); \
      INSN_END(2); \
  } \
  { \
    uint32_t reg; \
    _legacy_addr_t mem_op; \
    case opcode ## x: \
      code; \
      core->gpr[insn.mem.reg] = reg; \
      INSN_END(2); \
    case opcode ## l: \
      mem_op = parse_mem(core, insn); \
      CACHE_CHECK(core, mem_op, 4); \
      code; \
      CACHE_WRITE(core, mem_op, 4, reg); \
      INSN_END(2); \
  } \
}
#define INSN_RW(opcode, code) { \
  _legacy_addr_t mem_op; \
  { \
    uint16_t reg; \
    case opcode ## r: \
      reg = core->gpr[insn.mem.reg]; \
      code; \
      core->gpr[insn.mem.reg] = reg; \
      INSN_END(2); \
    case opcode ## m: \
      mem_op = parse_mem(core, insn); \
      CACHE_CHECK(core, mem_op, 2); \
      CACHE_READ(core, mem_op, 2, reg); \
      code; \
      CACHE_WRITE(core, mem_op, 2, reg); \
      INSN_END(2); \
  } \
  { \
    uint32_t reg; \
    case opcode ## x: \
      reg = core->gpr[insn.mem.reg]; \
      code; \
      core->gpr[insn.mem.reg] = reg; \
      INSN_END(2); \
    case opcode ## l: \
      mem_op = parse_mem(core, insn); \
      CACHE_CHECK(core, mem_op, 4); \
      CACHE_READ(core, mem_op, 4, reg); \
      code; \
      CACHE_WRITE(core, mem_op, 4, reg); \
      INSN_END(2); \
  } \
}
#define INSN_CONST(opcode, code) { \
  uint16_t value; \
  case opcode ## r: \
    value = core->gpr[insn.mem.reg]; \
    code; \
    INSN_END(2); \
  case opcode ## i: \
    value = insn.imm.imm; \
    code; \
    INSN_END(3); \
}
#define INSN_S_CONST(opcode, code) { \
  uint8_t value; \
  case opcode ## r: \
    value = core->gpr[insn.mem.reg]; \
    code; \
    INSN_END(2); \
  case opcode ## i: \
    value = insn.imm.imm; \
    code; \
    INSN_END(2); \
}
#define INSN_SC(opcode, condition, branch, suffix) { \
  case opcode: \
    if (condition) branch; \
    suffix; \
}
#define INSN_CC(opcode, status, branch, suffix) { \
  INSN_SC(opcode ## NO, !ST_TEST(status, CPU_OF), branch, suffix); \
  INSN_SC(opcode ## O, ST_TEST(status, CPU_OF), branch, suffix); \
  INSN_SC(opcode ## NZ, !ST_TEST(status, CPU_ZF), branch, suffix); \
  INSN_SC(opcode ## Z, ST_TEST(status, CPU_ZF), branch, suffix); \
  INSN_SC(opcode ## NS, !ST_TEST(status, CPU_SF), branch, suffix); \
  INSN_SC(opcode ## S, ST_TEST(status, CPU_SF), branch, suffix); \
  INSN_SC(opcode ## AE, !ST_TEST(status, CPU_CF), branch, suffix); \
  INSN_SC(opcode ## B, ST_TEST(status, CPU_CF), branch, suffix); \
  INSN_SC(opcode ## A, !ST_TEST(status, CPU_CF) && !ST_TEST(status, CPU_ZF), branch, suffix); \
  INSN_SC(opcode ## BE, ST_TEST(status, CPU_CF) || ST_TEST(status, CPU_ZF), branch, suffix); \
  INSN_SC(opcode ## GE, !!ST_TEST(status, CPU_SF) == !!ST_TEST(status, CPU_OF), branch, suffix); \
  INSN_SC(opcode ## L, !!ST_TEST(status, CPU_SF) != !!ST_TEST(status, CPU_OF), branch, suffix); \
  INSN_SC(opcode ## LE, (!!ST_TEST(status, CPU_SF) != !!ST_TEST(status, CPU_OF)) || ST_TEST(status, CPU_ZF), branch, suffix); \
  INSN_SC(opcode ## G, (!!ST_TEST(status, CPU_SF) == !!ST_TEST(status, CPU_OF)) && !ST_TEST(status, CPU_ZF), branch, suffix); \
}

#define CACHE_CHECK(core, addr, size) {\
  _legacy_cache_t *line1 = &(core->cache_lines[addr.cache.line]); \
  if (!line1->valid || (line1->tag != addr.cache.tag)) {\
    dtr_cache(core, addr); \
    return; \
  } \
  _legacy_addr_t next = {addr.raw + size - 1}; \
  if (next.cache.line != addr.cache.line) { \
    _legacy_cache_t *line2 = &(core->cache_lines[next.cache.line]); \
    if (!line2->valid || (line2->tag != next.cache.tag)) {\
      dtr_cache(core, next); \
      return; \
    } \
  } \
}
#define CACHE_READ(core, addr, size, to) {\
  _legacy_addr_t next = {addr.raw + size - 1}; \
  if (next.cache.line != addr.cache.line) { \
    _legacy_cache_t *line1 = &(core->cache_lines[addr.cache.line]); \
    _legacy_cache_t *line2 = &(core->cache_lines[next.cache.line]); \
    to = (line1->data[31] >> (8*addr.cache.byte)) | (line2->data[0] << (8*(4-(next.cache.byte+1)))); \
  } else { \
    _legacy_cache_t *line = &(core->cache_lines[addr.cache.line]); \
    to = *(uint32_t*)(((uint8_t*)(line->data + addr.cache.offset)) + addr.cache.byte); \
  } \
}
#define CACHE_WRITE(core, addr, size, from) {\
  _legacy_addr_t next = {addr.raw + size - 1}; \
  if (next.cache.line != addr.cache.line) { \
    _legacy_cache_t *line1 = &(core->cache_lines[addr.cache.line]); \
    _legacy_cache_t *line2 = &(core->cache_lines[next.cache.line]); \
    line1->data[31] = (line1->data[31] & DYN_MASK(8*addr.cache.line)) | (from << (8*addr.cache.line)); \
    line2->data[0] = (line2->data[0] & ~DYN_MASK(8*(next.cache.line+1))) | (from >> (4-(next.cache.byte+1))); \
    line1->dirty = 1; \
    line2->dirty = 1; \
  } else { \
    _legacy_cache_t *line = &(core->cache_lines[addr.cache.line]); \
    *(uint32_t*)(((uint8_t*)(line->data + addr.cache.offset)) + addr.cache.byte) = from; \
    line->dirty = 1; \
  } \
}

inline void dtr_intp(kyanite_legacy_cpu_t *core, uint8_t id) {
  ST_UNSET(core->basic, CPU_DTR_ACTIVE);
  ST_UNSET(core->basic, CPU_SLEEP_ACTIVE);
  ST_SET(core->basic, CPU_DTR_INTP);
  core->intp_status = id;
}
inline void dtr_mem_read(kyanite_legacy_cpu_t *core, uint16_t addr) {
  ST_SET(core->basic, CPU_DTR_MEM);
  core->bus_status = 0x0000;
  core->bus_address = addr;
}
inline void dtr_mem_write(kyanite_legacy_cpu_t *core, uint16_t addr, uint32_t data) {
  ST_SET(core->basic, CPU_DTR_MEM);
  core->bus_status = 0x1000;
  core->bus_address = addr;
  core->bus_data = data;
}
inline void dtr_cache(kyanite_legacy_cpu_t *core, _legacy_addr_t addr) {
  ST_SET(core->basic, CPU_DTR_CACHE);
  core->cache_status = 0;
  core->cache_address = addr;
}
inline _legacy_addr_t parse_mem(kyanite_legacy_cpu_t *core, _legacy_insn_t insn) __attribute_pure__ {
  _legacy_insn_offset_t addr = insn.mem.offset;
  switch (insn.mem.mode) {
    case INSN_MEM_GLOBAL:
      core->ip.raw += 2;
      return (_legacy_addr_t) {addr.imm};
    case INSN_MEM_LOCAL:
      core->ip.raw += 2;
      return (_legacy_addr_t) {addr.imm + core->ip.raw};
    case INSN_MEM_DIRECT:
      core->ip.raw += 1;
      return (_legacy_addr_t) {core->gpr[addr.reg]};
    case INSN_MEM_INDIRECT:
      core->ip.raw += 1;
      return (_legacy_addr_t) {core->gpr[addr.reg] + core->ip.raw};
    case INSN_MEM_PURE:
      core->ip.raw += 2;
      return (_legacy_addr_t) {core->gpr[addr.reg] + addr.micro.offset};
    case INSN_MEM_IMPURE:
      core->ip.raw += 2;
      return (_legacy_addr_t) {core->gpr[addr.reg] + addr.micro.offset + core->ip.raw};
    case INSN_MEM_MEMBER:
      core->ip.raw += 2;
      return (_legacy_addr_t) {core->gpr[addr.reg] + (core->gpr[addr.member.member] * addr.member.scalar)};
    default:
      return core->ip;
  }  
}

//Hardware executors
void kyanite_legacy_board_executor(device_t *empty, kyanite_legacy_board_t *board) {
  if (!ST_TEST(board->basic, IO_ACTIVE)) return;
  if (ST_TEST(board->basic, IO_INPUT)) {
    _legacy_addr_t f_addr = {board->basic.bus_address};
    uint8_t device = ST_TEST(board->basic, BOARD_BUS_IO) ? board->io_ids[f_addr.io.id] : board->ram_id;
    uint16_t address = ST_TEST(board->basic, BOARD_BUS_IO) ? f_addr.io.offset : board->basic.bus_address;
    if (device == 0xFF) { //IO absent, interrupt
      BUS_RESP((&board->basic), INTP, INT_IO);
      ST_UNSET(board->basic, IO_INPUT);
      return;
    }
    kyanite_legacy_bus_t *dev_ptr = (kyanite_legacy_bus_t*) board->basic.devices[device];
    if (dev_ptr->status & STATUS_KYANITE_IO_INPUT) { //IO busy, reject request
      kyanite_legacy_cpu_t *cpu = (kyanite_legacy_cpu_t*) board->basic.devices[board->cpu_id];
      ST_UNSET(cpu->basic, CPU_SLEEP_MEM);
      ST_UNSET(board->basic, IO_INPUT);
      return;
    }
    BUS_REQM(dev_ptr, ST_TEST(board->basic, IO_INPUT), address, board->basic.bus_in);
    ST_UNSET(board->basic, IO_INPUT);
  }
  if (ST_TEST(board->basic, IO_OUTPUT)) {
    kyanite_legacy_cpu_t *cpu = (kyanite_legacy_cpu_t*) board->basic.devices[board->cpu_id];
    ST_UNSET(cpu->basic, CPU_SLEEP_DELAY);
    ST_UNSET(cpu->basic, CPU_SLEEP_MEM);
    if (ST_TEST(board->basic, IO_RESPONSE)) {
      ST_UNSET(cpu->basic, CPU_DTR_MEM);
      cpu->bus_data = board->basic.bus_out;
    } else if (ST_TEST(board->basic, IO_INTP))
      dtr_intp(cpu, board->basic.bus_out);
    ST_UNSET(board->basic, IO_OUTPUT);
  }
}
void kyanite_legacy_hub_executor(kyanite_legacy_bus_t *s, kyanite_legacy_hub_t *hub) {
  if (!ST_TEST(hub->basic, IO_ACTIVE)) return;
  if (ST_TEST(hub->basic, IO_INPUT)) {
    uint8_t device = (hub->basic.bus_address >> 14) & 0x3;
    if (hub->ports[device] == 0xFF) { //IO absent, interrupt
      BUS_RESP(s, INTP, INT_IO);
      ST_UNSET(hub->basic, IO_INPUT);
      return;
    }
    kyanite_bus_t *dev_ptr = (kyanite_bus_t*) hub->basic.devices[hub->ports[device]];
    BUS_REQM(dev_ptr, ST_TEST(hub->basic, IO_INPUT), hub->basic.bus_address + hub->offset[device], hub->basic.bus_in);
    ST_UNSET(hub->basic, IO_INPUT);
  }
  if (ST_TEST(hub->basic, IO_OUTPUT)) {
    BUS_RESPM(s, ST_TEST(hub->basic, IO_OUTPUT), hub->basic.bus_out);
    ST_UNSET(hub->basic, IO_OUTPUT);
  } 
}
void kyanite_legacy_timer_executor(kyanite_legacy_cpu_t *cpu, kyanite_timer_t *timer) {
  //Timer counter update and test
  if (ST_TEST(timer->basic, TIMER_AUTO)) timer->counter++;
  if (timer->counter < timer->limit) return;

  //Timer control
  if (ST_TEST(timer->basic, TIMER_CLEAR)) timer->counter = 0;
  if (ST_TEST(timer->basic, TIMER_STOP)) timer->basic.status &= ~STATUS_RUNNING;

  //CPU control
  if (ST_TEST(timer->basic, TIMER_WAKEUP) && 
      ST_TEST(cpu->basic, CPU_SLEEP_TIMER) &&
      (cpu->delay == timer->id)) ST_UNSET(cpu->basic, CPU_SLEEP_TIMER);
  if (ST_TEST(timer->basic, TIMER_WATCHDOG)) cpu->basic.status &= ~STATUS_RUNNING;
  else if (ST_TEST(timer->basic, TIMER_TSWITCH)) dtr_intp(cpu, INT_TS);
  else if (ST_TEST(timer->basic, TIMER_INTP)) dtr_intp(cpu, INT_TIMER0 + timer->id);
}
void kyanite_legacy_ram_executor(kyanite_legacy_bus_t *s, kyanite_legacy_ram_t *ram) {
  if (!ST_TEST(ram->basic, IO_ACTIVE)) return;
  if (ram->basic.bus_address < ram->ram_size) {
    if (ST_TEST(ram->basic, IO_WRITE))
      ram->ram_data[ram->basic.bus_address] = ram->basic.bus_in;
    else
      BUS_RESP(s, RESPONSE, ram->ram_data[ram->basic.bus_address]);
  } else
    BUS_RESP(s, INTP, INT_MR);
  ST_UNSET(ram->basic, IO_ACTIVE);
}

void kyanite_legacy_cpu_executor(kyanite_legacy_board_t *s, kyanite_legacy_cpu_t *core) {
  if (core->basic.status & STATUS_KYANITE_CPU_SLEEP_ACTIVE) {
    if (core->basic.status & STATUS_KYANITE_CPU_SLEEP_DELAY) {
      core->delay--;
      if (core->delay == 0) 
	core->basic.status &= ~STATUS_KYANITE_CPU_SLEEP_DELAY;
    }
  } else if (core->basic.status & STATUS_KYANITE_CPU_DTR_ACTIVE) {
    if (ST_TEST(core->basic, CPU_DTR_MEM)) {
      BUS_REQM((&s->basic), (core->bus_status & 0x1000) ? ST(IO_WRITE) : ST(IO_READ), core->bus_address, core->bus_data);
      if (core->bus_status & 0x2000) ST_SET(s->basic, BOARD_BUS_IO);
      if (!(core->bus_status & 0x1000)) {
	if (core->bus_status < 0xFF) core->bus_status++;
	core->delay = core->bus_status & 0xFF;
	ST_SET(core->basic, CPU_SLEEP_DELAY);
	ST_SET(core->basic, CPU_SLEEP_MEM);
      } else {
	ST_UNSET(core->basic, CPU_DTR_MEM);
      }
    } else if (ST_TEST(core->basic, CPU_DTR_CACHE)) {
      _legacy_cache_t *line = &(core->cache_lines[core->cache_address.cache.line]);
      if (core->cache_status < 32) {
	if (core->cache_status == 0 && (!line->valid || !line->dirty)) {
	  core->cache_status = 32;
	  return;
	}
	_legacy_addr_t bus_addr = {.cache = {0, core->cache_status, core->cache_address.cache.line, line->tag}};
	dtr_mem_write(core, bus_addr.raw >> 2, line->data[core->cache_status]);
	core->cache_status++;
      } else if (core->cache_status < 64){
	if (core->cache_status == 32) 
	  line->valid = 0;	  
	else 
	  line->data[core->cache_status - 33] = core->bus_data;
	dtr_mem_read(core, (core->cache_address.raw >> 2) + core->cache_status);
	core->cache_status++;
      } else {
	line->data[core->cache_status - 33] = core->bus_data;
	line->valid = 1;
	line->dirty = 0;
	line->tag = core->cache_address.cache.tag;
	ST_UNSET(core->basic, CPU_DTR_CACHE);
      }
    } else if (ST_TEST(core->basic, CPU_DTR_INTP)) {
      if (!ST_TEST(core->basic, CPU_INTF) && ((core->intp_status & 0xFF) > INT_LAST_NMI)) {
	ST_UNSET(core->basic, CPU_DTR_INTP);
	return;
      }
      if (!(core->intp_status & 0x1000)) {
	if (ST_TEST(core->basic, CPU_TFAULT)) {
	  core->basic.status &= ~STATUS_RUNNING;
	  return;
	} else if (ST_TEST(core->basic, CPU_DFAULT)) {
	  ST_SET(core->basic, CPU_TFAULT);
	  core->intp_status = INT_DF;
	} else {
	  ST_SET(core->basic, CPU_DFAULT);
	}
	core->intp_status |= 0x1000;
      }
      _legacy_addr_t addr = {(core->intp_status & 0xFF) * 2};
      _legacy_addr_t stack = {KYANITE_GPR(SP) -= 2};
      uint16_t intp_ptr;
      CACHE_CHECK(core, addr, 2);
      CACHE_CHECK(core, stack, 2);
      CACHE_READ(core, addr, 2, intp_ptr);
      CACHE_WRITE(core, stack, 2, core->ip.raw);
      core->ip = (_legacy_addr_t) {intp_ptr};
      ST_UNSET(core->basic, CPU_SLEEP_ACTIVE);
      ST_UNSET(core->basic, CPU_INTF);
      ST_UNSET(core->basic, CPU_DTR_INTP);
    }
  } else {
    core->iparser(core);
  }
}

void kyanite_legacy_cpu_normal_parser(kyanite_legacy_cpu_t *core) {
  uint32_t raw_insn;
  CACHE_READ(core, core->ip, 4, raw_insn);
  _legacy_insn_t insn = {raw_insn};
  switch (insn.opcode) {
    //ALU operations
    INSN_ALU(ADD, dest += src);
    INSN_ALU(ADC, dest += (src + !!ST_TEST(core->basic, CPU_CF)));
    INSN_ALU(SUB, dest -= src);
    INSN_ALU(SBC, dest -= (src + !!ST_TEST(core->basic, CPU_CF)));
    INSN_ALU(MUL, dest *= src);
    INSN_ALU(DIV, if (!src) { dtr_intp(core, INT_DV); return; } dest /= src);
    INSN_ALU(MOD, if (!src) { dtr_intp(core, INT_DV); return; } dest %= src);
    INSN_ALU(CMP, ); //TODO
    INSN_ALU(AND, dest &= src);
    INSN_ALU(OR, dest |= src);
    INSN_ALU(XOR, dest ^= src);
    INSN_ALU(TEST, ); //TODO
    INSN_BIT(SHL, reg <<= index);
    INSN_BIT(SHR, reg >>= index);
    INSN_BIT(ROL, reg = (reg >> ((sizeof(reg) << 3) - index)) | (reg << index));
    INSN_BIT(ROR, reg = (reg << ((sizeof(reg) << 3) - index)) | (reg >> index));
    INSN_BIT(BT, ); //TODO
    INSN_BIT(BTS, reg |= (1 << index));
    INSN_BIT(BTR, reg &= ~(1 << index));
    INSN_BIT(BTC, reg ^= (1 << index));
    INSN_RW(INC, reg++);
    INSN_RW(DEC, reg--);
    INSN_RW(NOT, reg = ~reg);
    INSN_RW(NEG, reg = -reg);
    INSN_ALU_IMM(_BIT,); //TODO
    INSN_ALU_IMM(_ALU,); //TODO

    //Timing operations
    INSN_PURE(NOP, );
    INSN_PURE(HALT, core->basic.status &= ~STATUS_RUNNING);
    INSN_CONST(SLEEP, ST_SET(core->basic, CPU_SLEEP_DELAY); core->delay = value);
    {
      kyanite_timer_t *timer;
      case WAITi:
	ST_SET(core->basic, CPU_SLEEP_TIMER);
	core->delay = (uint8_t) insn.imm.imm;
	INSN_END(2);
      case TINCi:
      timer = (kyanite_timer_t*) core->basic.devices[core->timer_ids[insn.imm.imm & 0x3]];
	if (ST_TEST(timer->basic, TIMER_WATCHDOG)) timer->counter = 0;
	else timer->counter++;
	INSN_END(2);
    }
    INSN_PURE(SEI, ST_SET(core->basic, CPU_INTF));
    INSN_PURE(CLI, ST_UNSET(core->basic, CPU_INTF));
    INSN_S_CONST(TSW, {
      _legacy_task_t *current;
      _legacy_task_t *next;
      current = &core->task_slots[core->active_task];
      core->active_task = value & 0xF;
      next = &core->task_slots[core->active_task];
      current->ip = core->ip;
      current->status = core->basic.status;
      memcpy(current->gpr, core->gpr, 4 * KYANITE_GPR_LAST);
      core->ip = next->ip;
      core->basic.status = next->status;
      memcpy(core->gpr, next->gpr, 4 * KYANITE_GPR_LAST);
	})
    INSN_S_CONST(TMK, core->active_task = value & 0xF);

    //Conditionals
    INSN_CC(MOV, core->basic, ,);
    INSN_CC(J, core->basic, ,);

    //Control Flow
    {
      case CALLm:
      case JMPm:
	core->ip = parse_mem(core, insn);
	break;
      case LOOPrm:
	if (core->gpr[insn.mem.reg] != 0) { core->gpr[insn.mem.reg]--; return; }
	core->ip = parse_mem(core, insn);
	break;
      case INTi:
	dtr_intp(core, insn.imm.imm);
	break;
      case IRET:
      case RET:
	break;
    }

    //Data Flow
    INSN_ALU(MOV, dest = src);
    INSN_ALU_IMM(MOV, dest = src);
    INSN_READ(PUSH,); //TODO
    INSN_WRITE(POP,); //TODO
    {
      uint32_t inter;
      case SWAPxx:
	inter = core->gpr[insn.mem.reg];
	core->gpr[insn.mem.reg] = core->gpr[insn.mem.mode];
	core->gpr[insn.mem.mode] = inter;
	INSN_END(1);
    }
    {
      case LEArm:
	core->gpr[insn.mem.reg] = parse_mem(core, insn).raw;
	INSN_END(1);
    }
    {
      case CRLDx:
      case CRSTx:
	break;
    }
    {
      case INxi:
      case INxr:
	break;
    }
    {
      case OUTix:
      case OUTrx:
	break;
    }
    {
      case FLAGS:
	core->iparser = kyanite_legacy_cpu_flags_parser;
	INSN_END(1);
      case NOFLAGS:
	core->iparser = kyanite_legacy_cpu_normal_parser;
	INSN_END(1);
    }
    {
      case EXT:
	INSN_END(1);
    }
    {
      case LOCKi: //Invalid in this CPU
      case UNLKi:
      default:
	dtr_intp(core, INT_UD);
	break;
    }
  }
}

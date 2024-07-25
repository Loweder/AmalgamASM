#include <stdint.h>

#ifndef AMALGAM_ASM
#define AMALGAM_ASM

//TODO add more complex things like LEA
typedef enum {
  I_NOP,
  
  I_MOV, I_MOVI, I_FMOVI, I_SWAP, I_PUSH, I_POP,
 
  //TODO w/carry opcodes. Also CMP/TEST as flags
  I_ADD, I_ADI, I_SUB, I_SBI, 
  I_MUL, I_DIV, I_MOD, I_INC, I_DEC, 
  I_FADD, I_FSUB, I_FMUL, I_FDIV,
  I_I2F, I_F2I,

  I_XOR, I_XORI, I_OR, I_ORI, I_AND, I_ANDI, I_NOT, 
  I_BTS, I_BTR, I_BTC,
  I_SHL, I_SHR, I_ROL, I_ROR,
 
  //TODO jmp is also call, also maybe add SETcc
  I_INT, I_JMP, I_IJMP, I_JFL, I_RET,
  
  //TODO maybe make HALT as real opcode
  I_IO = 0x80, I_IIO, I_CR,
  I_SEI, I_CLI, I_IRET
} insn;

typedef enum {
 GPR_R1, GPR_R2, GPR_R3, GPR_R4, GPR_R5, GPR_R6, GPR_R7, 
 GPR_R8, GPR_R9, GPR_R10, GPR_R11, GPR_R12, GPR_R13,
 GPR_BP, GPR_SP, GPR_IP,
 GPR_LAST
} gpr;

typedef enum {
  MSR_ALU,
  MSR_MODE,
  MSR_MTASK,
  MSR_INT,
  MSR_PAGING,
  MSR_PAGE_FAULT,
  MSR_LAST
} msr;

//TODO ALU flags
//TODO Prev task on INT
#define MSR_ALU_CF 0
#define MSR_ALU_OF 1
#define MSR_ALU_ZF 2
#define MSR_ALU_SF 3
#define MSR_ALU_IF 4
#define MSR_MODE_FDIV 0
#define MSR_MODE_FDIV_MASK 0x0F
#define MSR_MODE_COREID 4
#define MSR_MODE_COREID_MASK 0xFF0
#define MSR_MODE_RUNNING 12
#define MSR_MODE_COMPLEX 13
#define MSR_MODE_KERNEL 14
#define MSR_INT_KERNEL 0
#define MSR_MTASK_ON 0
#define MSR_MTASK_PRIO 8
#define MSR_MTASK_PRIO_MASK 0xFF00
#define MSR_MTASK_ADDR 16
#define MSR_MTASK_ADDR_MASK 0xFFFFFF0000

#define INT_GP 0
#define INT_MR 1
#define INT_PF 2
#define INT_IO 3
#define INT_IL 4

#define GPR(name) core->gpr[GPR_ ## name]
#define MSR(name) core->msr[MSR_ ## name]
#define GPR_16(id) *(uint16_t*)(core->gpr + REG_OR(insn[id]))
#define GPR_32(id) *(uint32_t*)(core->gpr + REG_OR(insn[id]))
#define GPR_64(id) *(uint64_t*)(core->gpr + REG_OR(insn[id]))
#define MMUS_8(addr) mmu_simple(s, core, addr)
#define MMUS_16(addr) (uint16_t*) mmu_simple(s, core, addr)
#define MMUS_32(addr) (uint32_t*) mmu_simple(s, core, addr)
#define MMUS_64(addr) (uint64_t*) mmu_simple(s, core, addr)
#define MMUC_8(addr, mode) mmu_complex(s, core, addr, mode)
#define MMUC_16(addr, mode) (uint16_t*) mmu_complex(s, core, addr, mode)
#define MMUC_32(addr, mode) (uint32_t*) mmu_complex(s, core, addr, mode)
#define MMUC_64(addr, mode) (uint64_t*) mmu_complex(s, core, addr, mode)
#define IMM_16(start) *(uint16_t*)(insn + start)
#define IMM_32(start) *(uint32_t*)(insn + start)

#define BIT_M(dest, bit, val) dest = ((dest) & ~(1 << bit)) | ((val) << bit)
#define BIT_S(dest, bit) dest |= (1 << bit)
#define BIT_R(dest, bit) dest &= ~(1 << bit)
#define BIT_G(src, bit) (((src) >> bit) & 1)

#define REG_OR(on) (0x0F & on)
#define REG_RM(on) (0x30 & on)
#define REG_LN(on) (0x40 & on)
#define M8(on) (0xFFFFFF00 & on)
#define M10(on) (0xFFFFFC00 & on)
#define F8 0xFF
#define F10 0x3FF

#define MMU_READ 0
#define MMU_WRITE 1
#define MMU_EXEC 2

#define PG_AVAIL 0x01
#define PG_MODE 0x06
#define PG_SUPERV 0x08
#define PG_DEV 0x30
#define PG_MMU_PD(addr) ((addr & 0x03FC0000) >> 16)
#define PG_MMU_PE(addr) ((addr & 0x0003FC00) >> 8)
#define PG_MMU_ADDR(addr) (addr & 0x000003FF)

typedef enum {
  PORT_ROM, PORT_RAM, PORT_MAIN, PORT_IO, PORT_LAST
} port_type;

typedef enum {
  ERR_RAM_HZ = 0x0001, 
  ERR_RAM_MAX = 0x0002, 
  ERR_RAM_MIN = 0x0004, 
  ERR_MAIN_OL = 0x0008, 
  ERR_ROM_OL = 0x0010, 
  ERR_RAM_OL = 0x0020, 
  ERR_IO_OL = 0x0040,
  ERR_CPU_OL = 0x0100,
  ERR_CPU_IP = 0x0200,
  ERR_MALLOC = 0x8000,
  STATE_STABLE = 0x00010000,
  STATE_RUNNING = 0x00020000
} hw_state;

typedef enum {
  OPT_COMPLEX = 0x0001
} hw_opt;

typedef struct {
  port_type port;
  uint32_t freq;
  uint32_t p_size;
  uint8_t *(*builder)(void);
  void (*processor)(uint16_t, uint8_t*);
} md_desc;

typedef struct {
  uint32_t freq;
} cpu_desc;

typedef struct {
  uint32_t opts;
  uint8_t cpu_count;
  cpu_desc *cpus;
  uint8_t md_count[PORT_LAST];
  md_desc *md[PORT_LAST];
} hw_desc;

typedef struct {
  uint32_t max_freq;
  uint32_t freq;
  uint64_t gpr[GPR_LAST];
  uint32_t msr[MSR_LAST];
} cpu;

typedef struct {
  uint32_t status;
  uint32_t top_freq;
  uint8_t cpu_count;
  cpu *cpus;

  uint32_t ram_size;
  uint32_t ram_freq;
  uint8_t *ram;
  
  uint8_t disk_count;
  uint32_t disk_size[4];
  uint32_t disk_freq[4];
  uint8_t *disks[4];
  
  uint8_t rom_count;
  uint32_t rom_size[4];
  const uint8_t *roms[4];

  uint8_t io_count;
  uint32_t io_freq[8];
  void (*io_proc[8])(uint16_t, uint8_t*);
} hw;

static const uint8_t hw_limits[2][PORT_LAST] = {
  {
    [PORT_MAIN] = 1,
    [PORT_ROM] = 1,
    [PORT_RAM] = 2,
    [PORT_IO] = 4
  },
  {
    [PORT_MAIN] = 4,
    [PORT_ROM] = 4,
    [PORT_RAM] = 4,
    [PORT_IO] = 8
  }
};

void int_simple(hw *s, cpu *core, uint8_t number);
void int_complex(hw *s, cpu *core, uint8_t number);
uint8_t *mmu_simple(hw *s, cpu *core, uint16_t addr);
uint8_t *mmu_complex(hw *s, cpu *core, uint32_t addr, uint8_t mode);

#endif

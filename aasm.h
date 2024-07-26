#ifndef AMALGAM_ASM
#define AMALGAM_ASM

#include <stdint.h>
#include "aasm/hw.h"

//TODO add more complex things like LEA
typedef enum {
  I_NOP = 0x00, I_SLEEP, I_SLEEPI, I_GIPC,
  
  I_MOV = 0x10, I_MOVI, I_FMOVI, I_SWAP, I_PUSH, I_POP,
 
  //TODO w/carry opcodes. Also CMP/TEST as flags
  I_ADD = 0x20, I_ADI, I_SUB, I_SBI, 
  I_MUL, I_DIV, I_MOD, I_INC, I_DEC, 
  I_FADD, I_FSUB, I_FMUL, I_FDIV,
  I_I2F, I_F2I,

  I_XOR = 0x40, I_XORI, I_OR, I_ORI, I_AND, I_ANDI, I_NOT, 
  I_BTS, I_BTR, I_BTC,
  I_SHL, I_SHR, I_ROL, I_ROR,
 
  //TODO maybe add SETcc
  I_INT = 0x60, I_JMP, I_CALL, I_IJMP, I_ICALL, I_JC, I_JNC, I_RET,
  
  IP_FLAGS = 0xA0,

  //TODO maybe make HALT as real opcode
  I_IN = 0xC0, I_OUT, I_IIN, I_IOUT, I_CRLD, I_CRST,
  I_SEI, I_CLI, I_IRET
} insn;

typedef enum {
  JC = 0xF0, JO = 0xF1, JZ = 0xF2, JS = 0xF3, JGE = 0x31
} jcc;

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
  MSR_TIMER1,
  MSR_TIMER2,
  MSR_TIMERC1,
  MSR_TIMERC2,
  //RESERVED
  MSR_LAST = 0x10
} msr;

typedef enum {
  INT_GP,
  INT_MR,
  INT_PF,
  INT_IO,
  INT_IL,
  INT_TIMER1 = 0x20,
  INT_TIMER2
} intp;

//TODO More ALU flags
//TODO task switching
#define MSR_ALU_FLAG_MASK 0x0F
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
#define MSR_TIMER_ON 0
#define MSR_TIMER_MULT 4
#define MSR_TIMER_MULT_MASK 0x70
#define MSR_TIMER_VALUE 8
#define MSR_TIMER_VALUE_MASK 0xFFFFFF00

#define GPR(name) core->gpr[GPR_ ## name]
#define MSR(name) core->msr[MSR_ ## name]
#define GPR_16(id) *(uint16_t*)(core->gpr + REG_OR(insn[id]))
#define GPR_32(id) *(uint32_t*)(core->gpr + REG_OR(insn[id]))
#define GPR_64(id) *(uint64_t*)(core->gpr + REG_OR(insn[id]))
#define MSR_32(id) *(uint32_t*)(core->msr + REG_OR(insn[id]))
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
#define MASK_G(src, name) (((src) & name ## _MASK) >> name)

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

//TODO add more states
typedef enum {
  CPU_INT_FAIL 	= 0x0001,
  CPU_INT_R_FS 	= 0x0002,
  CPU_RUNNING 	= 0x00020000,
  CPU_COMPLEX 	= 0x00040000,
  CPU_ASLEEP 	= 0x00080000,
  CPU_DIRTY 	= 0x00100000,
  CPU_TIMER1 	= 0x00200000,
  CPU_TIMER2	= 0x00400000
} cpu_state;

typedef enum {
  HW_RAM_HZ 	= 0x0001, 
  HW_RAM_MAX 	= 0x0002, 
  HW_RAM_MIN 	= 0x0004, 
  HW_MAIN_OL 	= 0x0008, 
  HW_ROM_OL 	= 0x0010, 
  HW_RAM_OL 	= 0x0020, 
  HW_IO_OL 	= 0x0040,
  HW_CPU_OL 	= 0x0080,
  HW_CPU_IP 	= 0x0100,
  HW_MAIN_MALF 	= 0x0200,
  HW_ROM_MALF 	= 0x0400,
  HW_RAM_MALF 	= 0x0800,
  HW_MALLOC 	= 0x8000,
  HW_STABLE 	= 0x00010000,
  HW_RUNNING 	= 0x00020000,
  HW_COMPLEX 	= 0x00040000
} hw_state;

typedef struct {
  uint32_t status;
  uint32_t max_freq;
  uint32_t freq;
  uint32_t sleep;
  uint64_t ip_count;
  uint64_t gpr[GPR_LAST];
  uint32_t msr[MSR_LAST];
} cpu;

//Whatever you do, DO NOT USE DIRECTLY WITHOUT A POINTER
//FIXME convert CPUs to pointers? Or just convert registers? they take like 2KB now
typedef struct {
  uint32_t status;
  uint32_t ram_size;
  uint8_t cpu_count;
  uint8_t disk_count;
  uint8_t rom_count;
  uint8_t io_count;
  
  uint32_t ram_freq;
  uint32_t disk_size[MDD_C_MAIN_LIMIT];
  uint32_t disk_freq[MDD_C_MAIN_LIMIT];
  uint32_t rom_size[MDD_C_ROM_LIMIT];
  uint32_t io_freq[MDD_C_IO_LIMIT];
  
  uint8_t *ram;
  uint8_t *disks[MDD_C_MAIN_LIMIT];
  const uint8_t *roms[MDD_C_ROM_LIMIT];
  void (*io_proc[MDD_C_IO_LIMIT])(uint16_t, uint8_t*);
  
  cpu cpus[MDD_C_CPU_LIMIT];
} hw;

void int_simple(hw *s, cpu *core, uint8_t number);
void int_complex(hw *s, cpu *core, uint8_t number);
uint8_t *mmu_simple(hw *s, cpu *core, uint16_t addr);
uint8_t *mmu_complex(hw *s, cpu *core, uint32_t addr, uint8_t mode);
hw *build_system(const hw_desc *desc);
void execute(hw *s);

#endif

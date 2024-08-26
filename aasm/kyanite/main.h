#ifndef AMALGAM_ASM_KYANITE
#define AMALGAM_ASM_KYANITE

#include <aasm/aasm.h>

/* The Kyanite virtual architecture
 * Architecture for VM computers, has 2 modes: Simple (16-bit) and Complex (32-bit)
 * Shared features:
 * - Instruction unit:
 *     Has 16 GPRs
 *     Has 4 flags: CF, ZF, OF, SF
 *     ALU:
 *     - Arithmetics: ADD, SUB, MUL, DIV, MOD, INC, DEC, NEG
 *     - Logic: AND, OR, XOR, NOT
 *     - Bit manipulation: BTS, BTR, BTC
 *     - Offset: SHL, SHR, ROL, ROR
 *     - Tests: TEST, CMP, BT
 *     Control and data:
 *     - Jumps: JMP, CALL, RET
 *     - Conditionals: Jcc, JNcc, MOVcc, MOVNcc
 *     - Data: MOV, SWAP, LEA, PUSH, POP
 * - Timings and waiting:
 *     4-bit frequency divider on every component
 *     4 per-cpu and 2 system-wide 32-bit timers
 *     32 system-wide locks
 *     Timer counter: clock based, manual 
 *     Timer mode: clear, stop, no action 
 *     Timer commands: interrupt, wakeup, reset core, no effect
 *     Instructions:
 *     - Delays: NOP, SLEEP, GIPC, WAIT, TCNT
 *     - Concurrent locks: LOCK, UNLK
 * - Interrupts:
 *     System-wide and cpu-wide interrupts.
 *     256 interrupts.
 *     System-to-core interrupt mapping
 *     Reset on triple fault
 *     Global and interrupt-wide flags
 *     Instructions: INT, IRET, SEI, CLI
 * - Multitasking //TODO
 * - Debugging //TODO
 * - Direct IO and RAM access
 *     Board IO at port 0
 *     IO interrupts //TODO
 *     Instructions: IN, OUT, CRLD, CRST
 * Complex mode features:
 * - FPU arithmetics: FADD, FSUB, FMUL, FDIV, I2F, F2I
 * - Paged RAM/IO access (direct IO also valid)
 *     MCR instruction
 *     Page fault register
 * - Kernel and User access rings
 * - Builtin support for AES and checksums
 * */

typedef uint8_t (*kyanite_io_proc)(device_t *dev, uint32_t addr, uint8_t size, uint64_t *value, uint8_t mode);

enum {
  STATUS_KYANITE_BOARD_BIOS_MALF 	= 0x00000400,	//Error: malformed BIOS
  STATUS_KYANITE_BOARD_CPU_OL 		= 0x00001000,	//Error: too many cores
  STATUS_KYANITE_BOARD_RAM_OL 		= 0x00002000, 	//Error: too many ram banks
  STATUS_KYANITE_BOARD_IO_OL 		= 0x00004000,	//Error: too many IOs
  STATUS_KYANITE_BOARD_MALLOC 		= 0x00080000,	//Error: malloc()
  STATUS_KYANITE_BOARD_COMPLEX 		= 0x00100000,	//CPU supports Complex mode
  STATUS_KYANITE_CPU_FAULT	= 0x00000200,	//Processing an interrupt
  STATUS_KYANITE_CPU_DFAULT	= 0x00000400,	//Processing a double fault
  STATUS_KYANITE_CPU_TFAULT	= 0x00000400,	//Processing a triple fault
  STATUS_KYANITE_CPU_COMPLEX	= 0x00010000,	//In the Complex mode
  STATUS_KYANITE_CPU_KERNEL	= 0x00020000,	//In the Kernel ring
  STATUS_KYANITE_CPU_INTF	= 0x00040000,	//Interrupts enabled
  STATUS_KYANITE_CPU_SLEEP	= 0x00100000,	//Waiting for fixed amount
  STATUS_KYANITE_CPU_WAIT	= 0x00200000,	//Waiting for timer wakeup
  STATUS_KYANITE_CPU_LOCK	= 0x00400000,	//Waiting for lock acquisition
  STATUS_KYANITE_TIMER_CLEAR		= 0x00000100,	//Clear timer
  STATUS_KYANITE_TIMER_STOP		= 0x00000200,	//Stop timer
  STATUS_KYANITE_TIMER_INTERRUPT	= 0x00001000,	//Trigger interrupt
  STATUS_KYANITE_TIMER_WATCHDOG		= 0x00002000,	//Reset the system
  STATUS_KYANITE_TIMER_WAKEUP		= 0x00004000,	//Wakeup the system
  STATUS_KYANITE_IO_PRESENT	= 0x00010000	//IO is present
};
enum {
  KYANITE_GPR_R1, KYANITE_GPR_R2, KYANITE_GPR_R3, KYANITE_GPR_R4, 
  KYANITE_GPR_R5, KYANITE_GPR_R6, KYANITE_GPR_R7, KYANITE_GPR_R8, 
  KYANITE_GPR_R9, KYANITE_GPR_R10, KYANITE_GPR_R11, KYANITE_GPR_R12, 
  KYANITE_GPR_R13, KYANITE_GPR_BP, KYANITE_GPR_SP, KYANITE_GPR_IP,
  KYANITE_GPR_LAST
};
enum {
  KYANITE_BOARD_IO = 0,
  KYANITE_BOARD_RAM = 1,
  KYANITE_BOARD_TIMER0 = 2,
  KYANITE_BOARD_CPU0 = 4,
  KYANITE_CPU_TIMER0 = 0
};

#define KYANITE_GPR(name) (core->gpr[KYANITE_GPR_ ## name])

typedef struct { //Inherits device_t
  device_t basic;
  uint64_t ip_count;
  uint32_t delay;
  uint64_t gpr[KYANITE_GPR_LAST];
} kyanite_cpu_t;

typedef struct { //Inherits device_t
  device_t basic;
  uint32_t counter;
  uint32_t limit;
} kyanite_timer_t;

typedef struct { //Inherits device_t
  device_t basic;
  uint32_t locks;
} kyanite_board_t;

typedef struct { //Inherits device_t
  device_t basic;
} kyanite_io_t;

typedef struct { //Inherits device_t
  device_t basic;
  kyanite_io_proc processor;
} kyanite_io_dev_t;

typedef struct { //Inherits device_t
  kyanite_io_dev_t basic;
  uint8_t *data;
  uint32_t size;
} kyanite_ram_t;

void kyanite_cpu_executor(kyanite_board_t *s, kyanite_cpu_t *cpu);
void kyanite_timer_executor(device_t *, kyanite_timer_t *timer);
void kyanite_board_executor(device_t *, kyanite_board_t *s);
void kyanite_io_executor(kyanite_board_t *s, kyanite_io_t *io);
void kyanite_ram_executor(kyanite_board_t *s, kyanite_ram_t *ram);
uint8_t kyanite_ram_io_proc(kyanite_ram_t *dev, uint32_t addr, uint8_t size, uint64_t *value, uint8_t mode);

#endif

#ifndef AMALGAM_ASM_KYANITE
#define AMALGAM_ASM_KYANITE

#include <aasm/aasm.h>

/* The Kyanite virtual architecture
 * Architecture for VM computers, has 2 modes: Simple (16-bit) and Complex (32-bit)
 * Features:
 * - Instruction unit:
 *     Has 16 GPRs
 *     Has 4 flags: CF, ZF, OF, SF. Enabled with FLAGS prefix
 *     ALU:
 *     - Arithmetics: ADD, ADC, SUB, SBC, MUL, DIV, MOD, INC, DEC, NEG, CMP
 *     - Logic: AND, OR, XOR, NOT, TEST
 *     - Bit manipulation: BTS, BTR, BTC, BT
 *     - Offset: SHL, SHR, ROL, ROR
 *     Control and data:
 *     - Jumps: JMP, CALL, LOOP, RET
 *     - Conditionals: Jcc, MOVcc
 *     - Data: MOV, SWAP, LEA, PUSH, POP
 * - Timings and waiting:
 *     4-bit frequency divider on every component
 *     4 per-cpu 32-bit timers
 *     32 system-wide locks
 *     Timer counter: clock based, manual 
 *     Timer mode: clear, stop, no action 
 *     Timer commands: interrupt, wakeup, reset core, task switch, no effect
 *     Instructions:
 *     - Delays: NOP, HALT, SLEEP, WAIT, TINC
 *     - Concurrent locks: LOCK, UNLK
 * - Interrupts:
 *     System-wide and cpu-wide interrupts.
 *     256 interrupts.
 *     Reset on triple fault
 *     Global and interrupt-wide flags
 *     Instructions: INT, IRET, SEI, CLI
 * - Memory:
 *     128 lines of 128-byte cache
 *     256 entry TLB (Complex)
 *     Direct RAM and IO access (Simple)
 *     Paged RAM/IO and Direct IO access (Complex)
 *     Page fault register (Complex)
 *     IO interrupts
 *     Instructions: 
 *     - Memory: IN, OUT, CRLD, CRST
 *     - Cache: CCHLOAD, CCHSTORE, CCHPURGE
 *     - TLB: TLBLOAD, TLBERASE, TLBPURGE
 * - Multitasking:
 *     16 task slots
 *     All GPR registers saved
 *     Automatic/manual task switching
 *     Automatic page table switching
 *     Cache and TLB invalidated
 *     IVT unchanged
 *     TS interrupt vector
 *     Instructions:
 *     - TSW, TMK
 * - Debugging //TODO
 * Complex mode features:
 * - FPU arithmetics: FADD, FSUB, FMUL, FDIV, FCMP, I2F, F2I
 * - Kernel and User access rings
 * - Builtin support for AES and checksums
 * TODO extremely powerful, but extremely slow machines
 * TODO possibly redesign memory offsets
 * TODO make IP non-GPR
 * */

enum {
  STATUS_KYANITE_BOARD_ERR_BIOS_MALF 	= 0x00000100,	//Error: malformed BIOS
  STATUS_KYANITE_BOARD_ERR_CPU_OL 	= 0x00000200,	//Error: too many cores
  STATUS_KYANITE_BOARD_ERR_RAM_OL 	= 0x00000400, 	//Error: too many ram banks
  STATUS_KYANITE_BOARD_ERR_IO_OL 	= 0x00000800,	//Error: too many IOs
  STATUS_KYANITE_BOARD_ERR_MALLOC 	= 0x00001000,	//Error: out-of-simulation malloc()
  STATUS_KYANITE_BOARD_BUS_IO 		= 0x00100000,	//Memory bus is in the IO mode
  STATUS_KYANITE_IO_READ	= 0x00010000,	//IO is processing a memory read
  STATUS_KYANITE_IO_WRITE	= 0x00020000,	//IO is processing a memory write
  STATUS_KYANITE_IO_INPUT	= 0x00030000,	//IO is processing an input
  STATUS_KYANITE_IO_RESPONSE	= 0x00040000,	//Hub only. IO is processing a response
  STATUS_KYANITE_IO_INTP	= 0x00080000,	//Hub only. IO is processing an interrupt
  STATUS_KYANITE_IO_OUTPUT	= 0x000C0000,	//Hub only. IO is processing an output
  STATUS_KYANITE_IO_ACTIVE	= 0x000F0000,	//IO is processing a memory access
  STATUS_KYANITE_CPU_DTR_TLB		= 0x00000100,	//TLB fetch scheduled
  STATUS_KYANITE_CPU_DTR_CACHE		= 0x00000200,	//Cache fetch scheduled
  STATUS_KYANITE_CPU_DTR_INTP		= 0x00000400,	//Interrupt scheduled
  STATUS_KYANITE_CPU_DTR_MEM		= 0x00000800,	//Memory access scheduled
  STATUS_KYANITE_CPU_DTR_ACTIVE		= 0x00000F00,	//Any detour is taken
  STATUS_KYANITE_CPU_DFAULT		= 0x00001000,	//Next interrupt is a double fault
  STATUS_KYANITE_CPU_TFAULT		= 0x00002000,	//Next interrupt is a triple fault
  STATUS_KYANITE_CPU_KERNEL		= 0x00004000,	//In the Kernel ring
  STATUS_KYANITE_CPU_INTF		= 0x00008000,	//Interrupts enabled
  STATUS_KYANITE_CPU_CF			= 0x00010000,	//Carry flag
  STATUS_KYANITE_CPU_ZF			= 0x00010000,	//Zero flag
  STATUS_KYANITE_CPU_OF			= 0x00010000,	//Overflow flag
  STATUS_KYANITE_CPU_SF			= 0x00010000,	//Sign flag
  STATUS_KYANITE_CPU_SLEEP_DELAY	= 0x00100000,	//Waiting for fixed amount
  STATUS_KYANITE_CPU_SLEEP_TIMER	= 0x00200000,	//Waiting for timer wakeup
  STATUS_KYANITE_CPU_SLEEP_LOCK		= 0x00400000,	//Waiting for lock acquisition
  STATUS_KYANITE_CPU_SLEEP_MEM		= 0x00800000,	//Waiting for memory operation result
  STATUS_KYANITE_CPU_SLEEP_ACTIVE	= 0x00F00000,	//Any sleep operation is active
  STATUS_KYANITE_TIMER_AUTO	= 0x00000100,	//Increment timer each cycle
  STATUS_KYANITE_TIMER_CLEAR	= 0x00000200,	//Clear timer
  STATUS_KYANITE_TIMER_STOP	= 0x00000400,	//Stop timer
  STATUS_KYANITE_TIMER_INTP	= 0x00001000,	//Trigger interrupt
  STATUS_KYANITE_TIMER_WATCHDOG	= 0x00002000,	//Reset the system
  STATUS_KYANITE_TIMER_WAKEUP	= 0x00004000,	//Wakeup the system
  STATUS_KYANITE_TIMER_TSWITCH	= 0x00008000,	//Switch task
};
enum {
  KYANITE_GPR_R1, KYANITE_GPR_R2, KYANITE_GPR_R3, KYANITE_GPR_R4, 
  KYANITE_GPR_R5, KYANITE_GPR_R6, KYANITE_GPR_R7, KYANITE_GPR_R8, 
  KYANITE_GPR_R9, KYANITE_GPR_R10, KYANITE_GPR_R11, KYANITE_GPR_R12, 
  KYANITE_GPR_R13, KYANITE_GPR_R14, KYANITE_GPR_BP, KYANITE_GPR_SP,
  KYANITE_GPR_LAST
};

#define KYANITE_GPR(name) (core->gpr[KYANITE_GPR_ ## name])


typedef struct _kyanite_timer { //Inherits device_t
  device_t basic;
  uint32_t counter;
  uint32_t limit;
  
  uint8_t id;
} kyanite_timer_t;

typedef struct _kyanite_bus { //Inherits device_t
  uint32_t status;	//Device status
  uint32_t freq_mod;	//System counter modulo
  exec_t executor;	//Device manager
  device_t **devices;	//Children list
  uint8_t device_count;	//Children count
  uint8_t freq_div;	//Firmware-managed counter divider

  uint8_t bus_icore;	//Bus caller core
  uint8_t bus_ocore;	//Bus caller core
  uint32_t bus_address;	//Bus address
  uint64_t bus_in;	//Bus input
  uint64_t bus_out;	//Bus output
} kyanite_bus_t;
typedef struct _kyanite_board { //Inherits kyanite_bus_t
  kyanite_bus_t basic;
  uint32_t locks;	//Concurrent locks
  uint8_t cpu_ids[8]; 	//If 0xFF, device absent. Otherwise index in child devices
  uint8_t io_ids[32]; 	//If 0xFF, device absent. Otherwise index in child devices
  uint8_t ram_id; 	//ID of first RAM bank
  uint8_t ram_width; 	//Bitwidth of RAM bank index (Total amount of RAM banks is 2^width)
} kyanite_board_t;
typedef struct _kyanite_ram { //Inherits kyanite_bus_t
  kyanite_bus_t basic;
  uint64_t *ram_data;
  uint32_t ram_size;
} kyanite_ram_t;
typedef struct _kyanite_hub_t { //Inherits kyanite_bus_t
  kyanite_bus_t basic;
  uint32_t offset[8];	//Offset of each device's IO space
  uint8_t ports[8]; 	//If 0xFF, device absent. Otherwise index in child devices
} kyanite_hub_t;

typedef struct _kyanite_cache {
  uint64_t valid: 1;	//Line is valid
  uint64_t dirty: 1;	//Line is dirty
uint64_t: 30;
  uint64_t tag: 18;	//Cache tag
uint64_t: 14;
  uint64_t data[16];
} _cache_t;
typedef struct _kyanite_page {
  uint64_t valid: 1;	//TLBE is valid
  uint64_t mode: 2;	//Write+Execute
  uint64_t ring: 1;	//S/U ring
  uint64_t io: 1;	//Address is IO
  uint64_t type: 1;	//Normal/Supreme
uint64_t: 6;
  uint64_t address: 20;	//TLB address
} _page_t;
typedef struct _kyanite_tlb {
  uint64_t valid: 1;	//TLBE is valid
  uint64_t mode: 2;	//Write+Execute
  uint64_t ring: 1;	//S/U ring
  uint64_t io: 1;	//Address is IO
  uint64_t type: 1;	//Normal/Supreme
uint64_t: 6;
  uint64_t address: 20;	//TLB address
  uint64_t tag: 12;	//TLB tag
uint64_t: 20;
} _tlb_t;
typedef union _kyanite_addr {
  uint32_t raw;
  struct {
    uint32_t offset: 12;
    uint32_t file: 10;
    uint32_t dir: 10;
  } normal;
  struct {
    uint32_t offset: 22;
    uint32_t dir: 10;
  } supreme;
  struct {
    uint32_t offset: 12;
    uint32_t line: 8;
    uint32_t tag: 12;
  } tlb;
  struct {
    uint32_t byte: 3;
    uint32_t offset: 4;
    uint32_t line: 7;
    uint32_t tag: 18;
  } cache;
  struct {
    uint32_t offset: 27;
    uint32_t id: 5;
  } io;
} _addr_t;

typedef struct _kyanite_cpu { //Inherits device_t
  device_t basic;
  uint64_t ip_count;
  uint64_t gpr[KYANITE_GPR_LAST];
  uint32_t delay; //Can be immediate (SLEEP), timer (WAIT), lock (LOCK)

  _cache_t *cache_lines;
  _tlb_t *tlb;
  uint64_t bus_response;
  uint32_t bus_address;
  _addr_t cache_address;
  uint16_t bus_status;
  uint8_t cache_pass;
  uint8_t intp_id;
  uint8_t tlb_level;
  
  //Internal data
  uint8_t id;		//Core id
  uint8_t timer_ids[4];	//If 0xFF, device absent. Otherwise index in child devices
} kyanite_cpu_t;


void kyanite_board_executor(device_t *, kyanite_board_t *s);
void kyanite_timer_executor(kyanite_cpu_t *cpu, kyanite_timer_t *timer);
void kyanite_ram_executor(kyanite_bus_t *s, kyanite_ram_t *ram);
void kyanite_hub_executor(kyanite_bus_t *s, kyanite_hub_t *hub);
void kyanite_simple_cpu_executor(kyanite_board_t *s, kyanite_cpu_t *cpu);
void kyanite_complex_cpu_executor(kyanite_board_t *s, kyanite_cpu_t *cpu);

#endif

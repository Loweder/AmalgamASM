#ifndef AMALGAM_ASM_KYANITE_LEGACY
#define AMALGAM_ASM_KYANITE_LEGACY

#include <aasm/kyanite/main.h>

/* The Kyanite "Legacy" devices.
 * Legacy devices only have Simple mode.
 * Other differences:
 * - 32-bit bit bus size instead of 64-bit
 * - Single core CPUs
 * - No concurrent locks
 * - Only 8 IO devices
 * - Only 8 cache lines
 *  
 * r - normal register. m - small memory. x - eXtended register. l - large memory. i - immediate
 *
 *      x0     x1     x2     x3     x4     x5     x6     x7     x8     x9     xA     xB     xC     xD     xE     xF
 *  0x  MOVrr  MOVmr  MOVrm  MOVri  MOVxx  MOVlx  MOVxl  MOVxi  CRLDx  CRSTx  SWAPxx LEArm  INxi   INxr   OUTix  OUTrx
 *  1x  MOVNO  MOVO   MOVNZ  MOVZ   MOVNS  MOVS                 MOVAE  MOVBE  MOVB   MOVA   MOVGE  MOVLE  MOVG   MOVL
 *  2x  PUSHr  PUSHm  PUSHx  PUSHl                              POPr   POPm   POPx   POPl
 *  3x  ADDrr  ADDmr  ADDrm  ADDxx  ADDlx  ADDxl  _BITri _BITxi ADCrr  ADCmr  ADCrm  ADCxx  ADClx  ADCxl _ALUri _ALUxi
 *  4x  SUBrr  SUBmr  SUBrm  SUBxx  SUBlx  SUBxl                SBCrr  SBCmr  SBCrm  SBCxx  SBClx  SBCxl
 *  5x  ANDrr  ANDmr  ANDrm  ANDxx  ANDlx  ANDxl  BTrr   BTxr   ORrr   ORmr   ORrm   ORxx   ORlx   ORxl   SHLrr  SHLxr
 *  6x  XORrr  XORmr  XORrm  XORxx  XORlx  XORxl  BTSrr  BTSxr  CMPrr  CMPmr  CMPrm  CMPxx  CMPlx  CMPxl  SHRrr  SHRxr
 *  7x  TESTrr TESTmr TESTrm TESTxx TESTlx TESTxl BTRrr  BTRxr  MULrr  MULmr  MULrm  MULxx  MULlx  MULxl  ROLrr  ROLxr
 *  8x  DIVrr  DIVmr  DIVrm  DIVxx  DIVlx  DIVxl  BTCrr  BTCxr  MODrr  MODmr  MODrm  MODxx  MODlx  MODxl  RORrr  RORxr
 *  9x  INCr   INCm   INCx   INCl   DECr   DECm   DECx   DECl   NOTr   NOTm   NOTx   NOTl   NEGr   NEGm   NEGx   NEGl
 *  Ax
 *  Bx  NOP    HALT   SLEEPr SLEEPi WAITi  TINCi  LOCKi  UNLKi  JMPm   CALLm  LOOPrm INTi   RET    IRET   SEI    CLI
 *  Cx  JNO    JO     JNZ    JZ     JNS    JS                   JAE    JBE    JB     JA     JGE    JLE    JG     JL
 *  Dx  TSWr   TSWi   TMKr   TMKi
 *  Ex  
 *  Fx  FLAGS NOFLAGS EXT
 *
 * Class A (binary): reg + reg/mem/imm	ADD, ADC, SUB, SBC, AND, OR, XOR, TEST, CMP, MUL, DIV, MOD
 * Class B (unary): reg/mem		INC, DEC, NOT, NEG
 * Class C (bit): reg + reg/8bit imm 	SHL, SHR, ROL, ROR, BT, BTS, BTR, BTC
 * Class D (data):			MOV, SWAP, LEA, IN/OUT, CRLD/CRST, PUSH/POP, MOVcc
 * Class E (control):			JMP, CALL, LOOP, INT, RET, IRET, SEI, CLI, Jcc, TSW, TMK, TSW
 * Class F (timings):			NOP, HALT, SLEEP, WAIT, TINC, LOCK, UNLK
 *	//TODO TLB/Cache, debug
 * */

typedef struct _kyanite_legacy_bus { //Inherits device_t
  uint32_t status;	//Device status
  uint32_t freq_mod;	//System counter modulo
  exec_t executor;	//Device manager
  device_t **devices;	//Children list
  uint8_t device_count;	//Children count
  uint8_t freq_div;	//Firmware-managed counter divider

  uint16_t bus_address;	//Bus address
  uint32_t bus_in;	//Bus input
  uint32_t bus_out;	//Bus output
} kyanite_legacy_bus_t;
typedef struct _kyanite_legacy_board { //Inherits kyanite_legacy_bus_t
  kyanite_legacy_bus_t basic;
  uint8_t cpu_id; 	//ID of the CPU
  uint8_t ram_id; 	//ID of the RAM bank
  uint8_t io_ids[8]; 	//If 0xFF, device absent. Otherwise index in child devices
} kyanite_legacy_board_t;
typedef struct _kyanite_legacy_ram { //Inherits kyanite_legacy_bus_t
  kyanite_legacy_bus_t basic;
  uint32_t *ram_data;
  uint16_t ram_size;
} kyanite_legacy_ram_t;
typedef struct _kyanite_legacy_hub { //Inherits kyanite_legacy_bus_t
  kyanite_legacy_bus_t basic;
  uint16_t offset[4];	//Offset of each device's IO space
  uint8_t ports[4]; 	//If 0xFF, device absent. Otherwise index in child devices
} kyanite_legacy_hub_t;

typedef struct _kyanite_legacy_cache {
  uint32_t valid: 1;	//Line is valid
  uint32_t dirty: 1;	//Line is dirty
uint32_t: 14;
  uint32_t tag: 6;	//Cache tag
uint32_t: 10;
  uint32_t data[32];
} _legacy_cache_t;
typedef union _kyanite_legacy_addr {
  uint16_t raw;
  struct {
    uint16_t byte: 2;
    uint16_t offset: 5;
    uint16_t line: 3;
    uint16_t tag: 6;
  } cache;
  struct {
    uint16_t offset: 13;
    uint16_t id: 3;
  } io;
} _legacy_addr_t;
typedef struct _kyanite_legacy_task {
  uint32_t gpr[KYANITE_GPR_LAST];
  uint32_t status;
  _legacy_addr_t ip;
} _legacy_task_t;

typedef struct _kyanite_legacy_cpu { //Inherits device_t
  device_t basic;
  uint32_t gpr[KYANITE_GPR_LAST];
  uint16_t bus_status;
  uint16_t bus_address;
  uint32_t bus_data;

  _legacy_cache_t *cache_lines;
  _legacy_task_t *task_slots;
  void(*iparser)(struct _kyanite_legacy_cpu*);

  uint16_t delay; //Can be immediate (SLEEP) or timer (WAIT)
  _legacy_addr_t ip;
  _legacy_addr_t cache_address;
  uint16_t intp_status;

  uint8_t cache_status;
  uint8_t core_id;
  uint8_t active_task;
  uint8_t timer_ids[4];	//If 0xFF, device absent. Otherwise index in child devices
} kyanite_legacy_cpu_t;


void kyanite_legacy_board_executor(device_t *, kyanite_legacy_board_t *s);
void kyanite_legacy_timer_executor(kyanite_legacy_cpu_t *cpu, kyanite_timer_t *timer);
void kyanite_legacy_ram_executor(kyanite_legacy_bus_t *s, kyanite_legacy_ram_t *ram);
void kyanite_legacy_hub_executor(kyanite_legacy_bus_t *s, kyanite_legacy_hub_t *hub);
void kyanite_legacy_cpu_executor(kyanite_legacy_board_t *s, kyanite_legacy_cpu_t *cpu);
void kyanite_legacy_cpu_normal_parser(kyanite_legacy_cpu_t *core);
void kyanite_legacy_cpu_flags_parser(kyanite_legacy_cpu_t *core);

#endif

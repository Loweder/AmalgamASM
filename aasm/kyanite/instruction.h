#ifndef AMALGAM_ASM_KYANITE_INSTRUCTION
#define AMALGAM_ASM_KYANITE_INSTRUCTION

#include "main.h"

/* Instruction structure for the Kyanite architecture (depicted in big-endian):
 * N-bit is 16 in simple and 32 in complex mode
 * D/S 0 -> source is memory. D/S 1 -> destination is memory
 * ATYPE 0 -> offset is immediate. ATYPE 1 -> offset is scaled register
 *
 * 2 memory operands	|1-bit|2-bit|1-bit|4-bit|1-bit|2-bit|1-bit|4-bit|16-bit|
 *			|EXT  |RSVD |D/S  |REG  |RSVD |MODE |ATYPE|REG2 |OFFSET| 
 *
 * 2 operands		|1-bit|3-bit|4-bit|4-bit|4-bit|
 *			|EXT  |RSVDE|DEST |RSVD |SRC  | 
 *
 * Operand + immediate	|1-bit|3-bit|4-bit|N-bit|
 *			|EXT  |RSVDE|DEST |IMM  | 
 *
 * 1 memory operand	|1-bit|2-bit|1-bit|4-bit|16-bit|
 *			|EXT  |MODE |ATYPE|DEST |OFFSET| 
 *
 * 1 operand		|1-bit|3-bit|4-bit|
 *			|EXT  |RSVDE|DEST | 
 *
 * Immediate		|N-bit|
 * 			|IMM  |
 *
 * Memory offset	|16-bit| or |8-bit|4-bit|4-bit|
 *          		|IMM   | or |RSVD |SCAL |REG  |
 *
 * */

enum {
  I_NOP = 0x00, I_SLEEP, I_SLEEPI, I_WAIT, I_GIPC,
  
  I_MOV = 0x10, I_MOVI, I_LEA, I_SWAP, I_PUSH, I_POP,
 
  //TODO w/carry opcodes
  I_ADD = 0x20, I_SUB, 
  I_MUL, I_DIV, I_MOD, I_INC, I_DEC, 
  I_FADD, I_FSUB, I_FMUL, I_FDIV,
  I_I2F, I_F2I,

  I_XOR = 0x40, I_OR, I_AND, I_NOT, 
  I_BTS, I_BTR, I_BTC, I_BT,
  I_SHL, I_SHR, I_ROL, I_ROR,
 
  I_INT = 0x60, I_JMP, I_CALL, 
  I_JC, I_JNC, I_MOVC, I_MOVNC, I_RET,
  
  IP_FLAGS = 0xA0, IP_REPEAT,

  I_IN = 0xC0, I_INI, I_OUT, I_OUTI, I_CRLD, I_CRST,
  I_CFETCH, I_CFLUSH, I_CPURGE,
  I_SEI, I_CLI, I_IRET
};

enum {
  INSN_MEM_REG,		//Use register directly
  INSN_MEM_LOCAL,	//Use offset relative to IP
  INSN_MEM_PURE,	//Use absolute address in register
  INSN_MEM_IMPURE,	//Use offset relative to address in register
};

typedef struct __attribute__((__packed__)) {
  uint8_t _rsvd: 1;
  uint8_t mode: 2;
  uint8_t offset_type: 1;
  uint8_t reg1: 4;
  union {
    uint16_t direct;
    struct {
      uint16_t _rsvd: 8;
      uint16_t scalar: 4;
      uint16_t reg2: 4;
    } scaled;
  } offset;
} insn_mem_t;

typedef struct { //Memory operand needs to be accessed via (insn.raw >> 16)
  uint64_t opcode: 8;
  uint64_t extended: 1;
  uint64_t _rsvd1: 2;
  uint64_t direction: 1;
  uint64_t reg1: 4;
} _kyanite_insn_mem_t;
typedef struct {
  uint8_t opcode: 8;
  uint8_t extended: 1;
  uint8_t _rsvd1: 3;
  uint8_t reg1: 4;
  uint8_t _rsvd2: 4;
  uint8_t reg2: 4;
} _kyanite_insn_reg_t;
typedef struct {
  uint8_t opcode: 8;
} _kyanite_insn_pure_t;

typedef union {
  uint64_t raw;
  _kyanite_insn_pure_t pure;
  _kyanite_insn_reg_t reg; //Can have 1-2 operands, or 0-1 operands and an immediate
  _kyanite_insn_mem_t mem; //Can have 0-1 operands and a memory offset
} insn_t;

uint8_t insn_read_address(kyanite_board_t *s, kyanite_cpu_t *core, insn_mem_t address, uint64_t *to, uint8_t extended);
uint8_t insn_write_address(kyanite_board_t *s, kyanite_cpu_t *core, insn_mem_t address, uint64_t *from, uint8_t extended);

#endif

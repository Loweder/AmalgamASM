#ifndef AMALGAM_ASM_KYANITE_INSTRUCTION
#define AMALGAM_ASM_KYANITE_INSTRUCTION

#include "main.h"

/* Instruction structure for the Kyanite architecture (depicted in big-endian):
 *
 * 2 memory 16-bit	|4-bit|4-bit|16-bit|
 * 			|REG1 |MODE |MEM   |
 *
 * 2 operands 16-bit	|4-bit|4-bit|
 * 			|REG1 |REG2 |
 * 
 * 1 memory 16-bit	|4-bit|4-bit|16-bit|
 * 			|RSVD |MODE |MEM   |
 *
 * 1 operand 16-bit	|4-bit|4-bit|
 * 			|REG  |OP   |
 *
 * 1 op, 1 imm 16-bit	|4-bit|4-bit|16-bit|
 * 			|REG  |OP   |IMM   |
 *
 * 1 imm 16-bit		|16-bit|
 * 			|IMM   |
 *
 * Memory 16-bit	|16-bit| or |4-bit|4-bit|8-bit| or |4-bit|12-bit| or |4-bit|4-bit|4-bit|4-bit|
 * 			|IMM   |    |REG  |RSVD |IMM  |    |REG  |RSVD  |    |REG  |RSVD |OFF  |SCAL |
 * */

enum __attribute__((packed)) {
  MOVrr, MOVmr, MOVrm, MOVri, MOVxx, MOVlx, MOVxl, MOVxi, CRLDx, CRSTx, SWAPxx,LEArm, INxi,  INxr,  OUTix, OUTrx,
  MOVNO, MOVO,  MOVNZ, MOVZ,  MOVNS, MOVS, _RVD15, _RVD16,MOVAE, MOVBE, MOVB,  MOVA,  MOVGE, MOVLE, MOVG,  MOVL,  
  PUSHr, PUSHm, PUSHx, PUSHl, _RVD1, _RVD2, _RVD3, _RVD4, POPr,  POPm,  POPx,  POPl,  _RVD5, _RVD6, _RVD7, _RVD8,
  ADDrr, ADDmr, ADDrm, ADDxx, ADDlx, ADDxl, _BITri,_BITxi,ADCrr, ADCmr, ADCrm, ADCxx, ADClx, ADCxl, _ALUri,_ALUxi,
  SUBrr, SUBmr, SUBrm, SUBxx, SUBlx, SUBxl, _RVD9, _RVD10,SBCrr, SBCmr, SBCrm, SBCxx, SBClx, SBCxl, _RVD11,_RVD12,
  ANDrr, ANDmr, ANDrm, ANDxx, ANDlx, ANDxl, BTrr,  BTxr,  ORrr,  ORmr,  ORrm,  ORxx,  ORlx,  ORxl,  SHLrr, SHLxr,
  XORrr, XORmr, XORrm, XORxx, XORlx, XORxl, BTSrr, BTSxr, CMPrr, CMPmr, CMPrm, CMPxx, CMPlx, CMPxl, SHRrr, SHRxr,
  TESTrr,TESTmr,TESTrm,TESTxx,TESTlx,TESTxl,BTRrr, BTRxr, MULrr, MULmr, MULrm, MULxx, MULlx, MULxl, ROLrr, ROLxr,
  DIVrr, DIVmr, DIVrm, DIVxx, DIVlx, DIVxl, BTCrr, BTCxr, MODrr, MODmr, MODrm, MODxx, MODlx, MODxl, RORrr, RORxr,
  INCr,  INCm,  INCx,  INCl,  DECr,  DECm,  DECx,  DECl,  NOTr,  NOTm,  NOTx,  NOTl,  NEGr,  NEGm,  NEGx,  NEGl,
  _RVD13 = 0xAF,
  NOP,   HALT,  SLEEPr,SLEEPi,WAITi, TINCi, LOCKi, UNLKi, JMPm,  CALLm, LOOPrm,INTi,  RET,   IRET,  SEI,   CLI,
  JNO,   JO,    JNZ,   JZ,    JNS,   JS,    _RVD17,_RVD18,JAE,   JBE,   JB,    JA,    JGE,   JLE,   JG,    JL,    
  TSWr,  TSWi,  TMKr,  TMKi,
  _RVD14 = 0xEF,
  FLAGS, NOFLAGS, EXT
};

enum {
  INSN_MEM_GLOBAL,	//Use immediate offset as absolute address
  INSN_MEM_LOCAL,	//Use immediate offset relative to IP
  INSN_MEM_DIRECT,	//Use address in register as absolute address
  INSN_MEM_INDIRECT,	//Use address in register relative to IP
  INSN_MEM_PURE,	//Use immediate offset added to address in register as absolute value
  INSN_MEM_IMPURE,	//Use immediate offset added to address in register relative to IP
  INSN_MEM_MEMBER,	//Use scaled register offset relative to address in register
  INSN_MEM_IP,		//Use current IP directly
};

typedef union _legacy_insn_offset {
  uint16_t imm;
  uint16_t reg: 4;
  struct {
    uint16_t _rsvd: 8;
    uint16_t offset: 8;
  } micro;
  struct {
    uint16_t _rsvd: 8;
    uint16_t member: 4;
    uint16_t scalar: 4;
  } member;
} _legacy_insn_offset_t;

typedef union _legacy_insn {
  uint32_t raw;
  uint8_t opcode;
  struct __attribute__((packed)) {
    uint8_t opcode;
    uint8_t reg: 4;
    uint8_t mode: 4;
    _legacy_insn_offset_t offset;
  } mem;
  struct __attribute__((packed)) {
    uint8_t opcode;
    uint8_t reg: 4;
    uint8_t op: 4;
    uint16_t imm;
  } opimm;
  struct __attribute__((packed)) {
    uint8_t opcode;
    uint16_t imm;
  } imm;
} _legacy_insn_t;

#endif

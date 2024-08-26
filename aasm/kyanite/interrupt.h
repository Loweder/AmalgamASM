#ifndef AMALGAM_ASM_KYANITE_INTERRUPT
#define AMALGAM_ASM_KYANITE_INTERRUPT

#include "main.h"

enum {
  INT_GP, 		//Generic protection error
  INT_IL,		//Illegal instruction error
  INT_MR,		//Memory access error
  INT_IO,		//IO access error
  INT_DV,		//Division error
  INT_PF,		//Page fault
  INT_LAST_NMI = 0x1F,	//Last non-maskable interrupt
};

void interrupt(kyanite_board_t *s, kyanite_cpu_t *core, uint8_t number);

#endif

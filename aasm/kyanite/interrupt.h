#ifndef AMALGAM_ASM_KYANITE_INTERRUPT
#define AMALGAM_ASM_KYANITE_INTERRUPT

#include "main.h"

enum {
  INT_GP, 		//Generic protection error
  INT_UD,		//Illegal instruction error
  INT_MR,		//Memory access error
  INT_IO,		//IO access error
  INT_DV,		//Division error
  INT_PF,		//Page fault
  INT_DF,		//Double fault
  INT_LAST_NMI = 0x1F,	//Last non-maskable interrupt
  INT_TIMER0,
  INT_TIMER1,
  INT_TIMER2,
  INT_TIMER3,
  INT_TS
};

#endif

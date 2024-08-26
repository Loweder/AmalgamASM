#ifndef AMALGAM_ASM
#define AMALGAM_ASM

#include <stdint.h>

/* TODO add more complex things like LEA
 * FIXME Makefile, properly use LD (currently invalid)
 * TODO Redesign
 * Test mmu_* functions, specifically test edge cases
 * Add cache to Complex processors, and add heavy penalty on memory reads
 * Other: make realistic GPUs. Also make a 4D minesweeper for it, cus why not?
 * Make "cooldown" field in CPUs, and allow it to be incremented dynamically.
 * Make memory access have long cooldown
 * Add "REPEAT" prefix (label: insn; DEC %r1; JNZ label;), and FLAGS to INC and DEC
 * Change system from "execute first, wait later" to "wait first, execute later". Applied to IO and RAM access
 * More ALU flags
 * TODO add more states
 * TODO detach and attach 'io's from 'hw'
 * TODO frequency
 * Assuming given a CPU with 2000, IOs with 520, 640 and 1080
 * Lets take the highest frequency: 2000. 
 * This means that CPU must run 2000/2000 times
 * And IOs must run 500/2000, 640/2000, 1080/2000
 * Lets find the needed frequency
 * CPU = 2000/2000 = 1 cycle
 * IOs = 2000/500 = 4 cycles, 2000/640=3.125 cycles, 2000/1080=1.851852 cycles.
 * Lets apply ceil()
 * IOs = 4 cycles, 4 cycles, 2 cycles
 * */

typedef struct device_t device_t;
typedef struct system_t system_t;

typedef void (*exec)(device_t *s, device_t *dev);

enum {
  STATUS_RUNNING = 0x01,	//Device is running
};

struct device_t { //Devices must contain "device_t basic" as their first field
  uint32_t status;	//Device status
  uint32_t freq_mod;	//System counter modulo
  exec executor;	//Device manager
  device_t **devices;	//Children list
  uint8_t device_count;	//Children count
  uint8_t freq_div;	//Firmware-managed counter divider
};

struct system_t { //Inherits "device_t"
  device_t basic;
  uint32_t counter;	//System counter
};

void execute(system_t *s);
#endif

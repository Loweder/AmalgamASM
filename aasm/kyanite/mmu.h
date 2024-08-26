#ifndef AMALGAM_ASM_KYANITE_MMU
#define AMALGAM_ASM_KYANITE_MMU

#include "main.h"

/* Complex Mode paging for the Kyanite architecture (depicted in big-engian):
 *
 * Page dir	|1-bit|2-bit|1-bit|5-bit|1-bit|2-bit|20-bit|
 * 		|AVAIL|W/E  |RING |RSVD |0    |RSVD |ADDR  |
 *
 * Page supreme	|1-bit|2-bit|1-bit|5-bit|1-bit|12-bit|10-bit|
 * 		|AVAIL|W/E  |RING |DEV  |1    |RSVD  |ADDR  |
 *
 * Page file	|1-bit|2-bit|1-bit|5-bit|3-bit|20-bit|
 * 		|AVAIL|W/E  |RING |DEV  |RSVD |ADDR  |
 *
 * Address	|12-bit|10-bit|10-bit|
 * 		|OFFSET|FILE  |DIR   |
 * */

typedef enum {
  MMU_READ 	= 0x0,		//Memory read
  MMU_WRITE	= 0x1,		//Memory write
  MMU_EXEC	= 0x2,		//Execute
  MMU_SYSRD	= 0x4,		//System read
  MMU_SYSWR 	= 0x5		//System write
} mmu_req_t;

typedef struct {
  uint32_t available: 1;
  uint32_t mode: 2;
  uint32_t ring: 1;
  uint32_t device: 5;
  uint32_t type: 1;
  uint32_t address: 22; 	//Partially reserved
} mmu_page_t;

typedef union {
  struct {
    uint32_t offset: 12;
    uint32_t file: 10;
    uint32_t dir: 10;
  } filed;
  struct {
    uint32_t offset: 22;
    uint32_t dir: 10;
  } supreme;
} mmu_addr_t;

#define MMU_PG_TABLE_MASK(table) (table & 0xFFFFF000)
#define MMU_PG_DIR_MASK(dir) 	((dir.address << 10) & 0xFFFFF000)
#define MMU_PG_FILE_MASK(file) 	((file.address << 10) & 0xFFFFF000)
#define MMU_PG_SUPREME_MASK(dir) ((dir.address << 10) & 0xFFC00000)

uint8_t mmu_io(kyanite_board_t *s, kyanite_cpu_t *core, uint16_t addr, uint8_t size, uint64_t *data, mmu_req_t task);
uint8_t mmu_ram(kyanite_board_t *s, kyanite_cpu_t *core, uint32_t addr, uint8_t size, uint64_t *data, mmu_req_t task);

#endif

#ifndef AMALGAM_ASM_HW
#define AMALGAM_ASM_HW

#include <stdint.h>

#define MDD_S_CPU_LIMIT 1
#define MDD_S_MAIN_LIMIT 1
#define MDD_S_ROM_LIMIT 1
#define MDD_S_RAM_LIMIT 2
#define MDD_S_IO_LIMIT 4
#define MDD_C_CPU_LIMIT 8
#define MDD_C_MAIN_LIMIT 4
#define MDD_C_ROM_LIMIT 4
#define MDD_C_RAM_LIMIT 4
#define MDD_C_IO_LIMIT 8

#define MDD_TEST_LIMIT(type) if (desc->md_count[MDD_PORT_ ## type] > \
    (desc->opts & HWD_OPT_COMPLEX ? MDD_C_ ## type ## _LIMIT : MDD_S_ ## type ## _LIMIT))

typedef void (*proc)(uint16_t, uint8_t*);

typedef enum {
  MDD_PORT_CPU, MDD_PORT_ROM, MDD_PORT_RAM, 
  MDD_PORT_MAIN, MDD_PORT_IO, MDD_PORT_LAST
} md_desc_type;

typedef enum {
  HWD_OPT_COMPLEX = 0x0001
} hw_desc_opt;

//TODO add userdata as void*
// Also do: add disks to IO space, or something like that
typedef struct {
  uint32_t freq;
  uint32_t p_size;
  uint8_t *(*builder)(void);
  proc processor;
} md_desc;

typedef struct {
  uint32_t opts;
  uint8_t md_count[MDD_PORT_LAST];
  md_desc *md[MDD_PORT_LAST];
  uint32_t p_size;
  uint8_t *(*builder)(void);
  proc processor;
} hw_desc;

#endif

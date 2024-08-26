#ifndef AMALGAM_ASM_KYANITE_BUILDER
#define AMALGAM_ASM_KYANITE_BUILDER

#include "main.h"

//Total IO count is: 1 for MOBO + K for actual IO = at most 32
#define MDD_S_CPU_LIMIT 1
#define MDD_S_RAM_LIMIT 2
#define MDD_S_IO_LIMIT 7
#define MDD_C_CPU_LIMIT 8
#define MDD_C_RAM_LIMIT 4
#define MDD_C_IO_LIMIT 31

#define MDD_TEST_LIMIT(type) if (desc->md_count[MDD_PORT_ ## type] > \
    (desc->opts & HWD_OPT_COMPLEX ? MDD_C_ ## type ## _LIMIT : MDD_S_ ## type ## _LIMIT))

typedef enum {
  MDD_PORT_CPU, MDD_PORT_RAM,
  MDD_PORT_IO, MDD_PORT_LAST
} md_desc_type;

typedef enum {
  HWD_OPT_COMPLEX = 0x0001
} hw_desc_opt;

typedef struct {
  uint32_t freq;
} cpu_desc;

typedef struct {
  uint32_t freq;
  uint32_t size;
} ram_desc;

typedef struct {
  uint32_t freq;
  kyanite_io_proc processor;
  void *udata;
} io_desc;

typedef struct {
  uint32_t opts;
  kyanite_io_proc processor;
  void *udata;
  uint8_t bios_id;
  uint8_t md_count[MDD_PORT_LAST];
  void *md[MDD_PORT_LAST];
} hw_desc;

system_t *build_kyanite(const hw_desc *desc);

#endif

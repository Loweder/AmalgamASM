#include <aasm/aasm.h>
#include <aasm/compile.h>
#include <aasm/util.h>
#include <stdlib.h>
#include <stdio.h>

const char *get_file(const char *name);
uint8_t *rom_mem(void);
uint8_t *hw_mem(void);

static const md_desc cpus[2] = {
  {100}, {100}
}; 

static const md_desc roms[1] = {
  {
    .freq = 100,
    .p_size = 0x1000,
    .builder = rom_mem,
    .processor = 0
  }
};

static const md_desc rams[1] = {
  {
    .freq = 100,
    .p_size = 0x4000,
    .builder = 0,
    .processor = 0
  }
};

static const hw_desc device = {
  .md_count = {[MDD_PORT_ROM] = 1, [MDD_PORT_RAM] = 1, [MDD_PORT_CPU] = 2},
  .md = {[MDD_PORT_ROM] = (md_desc*) roms, [MDD_PORT_RAM] = (md_desc*) rams, [MDD_PORT_CPU] = (md_desc*) cpus},
  .opts = HWD_OPT_COMPLEX,
  .p_size = 0,
  .builder = hw_mem,
  .processor = 0
};

#define DELIM "------------------------------------------------------------------------------------------------------------------------\n"

static cmpl_env env = {
  .get_file = get_file,
};

int main(void) {
  env.err = emalloc(100);
  l_list *product = preprocess("main.s", &env);
  if (!product) {
    printf("Preprocess error in %s\n", env.err);
    return -1;
  }
  while (product->data) {
    l_list *line = ll_shift(product);
    char *file_name = ll_shift(line);
    size_t line_number = (size_t) ll_shift(line);

    printf("%s:%ld ", file_name, line_number);
    free(file_name);

    while(line->data) {
      char *word = ll_shift(line);
      printf("%s ", word);
      free(word);
    }
    printf("\n");
    ll_free(line);
  }
}

int main_i(void) {
  hw *system = build_system(&device);

  if (!(system->status & HW_RUNNING)) {
    printf("System build failed: %X\n", system->status);
    return -1;
  }
  while (1) {
    printf(DELIM "System State:\n");
    for (int i = 0; i < system->cpu_count; i++) {
      cpu *core = system->cpus + i;
      printf(DELIM "Core %i:\nR1: %8lX | R2: %8lX | R3: %8lX | R4: %8lX | R5: %8lX | R6: %8lX | R7: %8lX | R8: %8lX\n\
R9: %8lX | 10: %8lX | 11: %8lX | 12: %8lX | 13: %8lX | BP: %8lX | SP: %8lX | IP: %8lX\n\n\
ALU: %8X | MODE: %8X | MTASK: %8X | INT: %8X | PAGING: %8X | PAGE-F: %8X\n", i, 
	  GPR(R1), GPR(R2), GPR(R3), GPR(R4), GPR(R5), GPR(R6), GPR(R7), GPR(R8), 
	  GPR(R9), GPR(R10), GPR(R11), GPR(R12), GPR(R13), GPR(BP), GPR(SP), GPR(IP),
	  MSR(ALU), MSR(MODE), MSR(MTASK), MSR(INT), MSR(PAGING), MSR(PAGE_FAULT));
    }
    getchar();
    execute(system);
  }
}

uint8_t *rom_mem(void) {
  static uint8_t data[0x1000] = {
    0xAC, 0x36, 0x18, 0xCA,
    [0x100] = I_MOVI, 0x00, 0x00, 0x10,
    I_MOVI, 0x01, 0xFE, 0xCA,
    I_MOVI, 0x02, 0x00, 0x20,
    I_MOVI, 0x22, 0x40, 0x30,
    I_MOV, 0x13, 0x02,
    I_PUSH, 0x01,
    I_POP, 0x04,
    I_SWAP, 0x02, 0x04,
    I_MOV, 0x55, 0x00
  };
  return data;
}

uint8_t *hw_mem(void) {

  static uint8_t data[1] = {
    0x00
  };
  return data;
}

const char *get_file(const char *name) {
  char *full_name = emalloc(256);
  snprintf(full_name, 256, "test/asm/%s", name);
  FILE *file = fopen(full_name, "r");
  fseek(file, 0L, SEEK_END);
  size_t size = ftell(file);
  char *data = emalloc(size);
  rewind(file);
  fread(data, size, 1, file);
  fclose(file);
  free(full_name);
  return data;
}

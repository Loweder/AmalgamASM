#include <aasm/aasm.h>
#include <aasm/compile.h>
#include <aasm/util.h>
#include <stdlib.h>
#include <stdio.h>

static void _debug_print_line(llist_t *line) {
  char *file_name = line->data->value;
  size_t line_number = (size_t) line->data->next->value;

  printf("%s:%ld ", file_name, line_number);

  for(struct _le *entry = line->data->next->next; entry; entry = entry->next) {
    printf("%s ", (char*) entry->value);
  }
  printf("\n");
}
static void _debug_print_lines(llist_t *lines) { //NOLINT
  for (struct _le *line = lines->data; line; line = line->next) {
    _debug_print_line(line->value);
  }
}

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

static cmpl_env_t env = {
  .get_file = get_file,
};

int main(void) {
  env.errors = ll_make();
  compiled_t *product = compile("main.s", &env);
  if (env.errors->size) {
    printf("Preprocess errors:\n");
    E_FOR(entry, env.errors->data) {
      printf("Error %s\n", (char*) entry->value);
    }
    return 1;
  }
  llist_t *symbols = hm_free_to(product->symbols);
  llist_t *sections = hm_free_to(product->sections);

  E_FOR(entry, symbols->data) {
    llist_t *l_entry = entry->value;
    symbol_t *symbol = l_entry->data->next->value;
    printf("Symbol '%s'. Section '%s', value '0x%lx', global '%d'\n", (char*) l_entry->data->value, symbol->section, symbol->value, symbol->global);
  }
  printf("\n");

  E_FOR(entry, sections->data) {
    printf("Section '%s'\n", (char*)((llist_t*)entry->value)->data->value);
    section_t *section = ((llist_t*)entry->value)->data->next->value;
    for (size_t i = 0; i < section->size; i++) {
      printf("%02x ", section->data[i]);
      if (!((i+1) % 32)) printf("\n");
    }
    if (section->size % 32) printf("\n");
    for (size_t i = 0; i < section->size; i++) {
      char datum = section->data[i];
      printf("%c", datum >= 0x20 && datum < 0x7F ? datum : '.');
      if (!((i+1) % 32)) printf("\n");
    }
    if (section->size % 32) printf("\n");
    printf("Requests:\n");
    E_FOR(req_entry, section->expr->data) {
      expr_info_t *request = req_entry->value;
      printf("Expression '%s'. At '0x%lx' of size %ld\n", request->expression, request->address, request->size);
    }
    printf("\n");
  }
  return 0;
}

int main_i(void) {
  hw_t *system = build_system(&device);

  if (!(system->status & HW_RUNNING)) {
    printf("System build failed: %X\n", system->status);
    return -1;
  }
  while (1) {
    printf(DELIM "System State:\n");
    for (int i = 0; i < system->cpu_count; i++) {
      cpu_t *core = system->cpus + i;
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
  static uint8_t data[1] = {
    0x00
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
  FILE *file = fopen(full_name, "rb");
  fseek(file, 0L, SEEK_END);
  size_t size = ftell(file);
  char *data = emalloc(size + 1);
  rewind(file);
  fread(data, size, 1, file);
  fclose(file);
  data[size] = '\0';
  free(full_name);
  return data;
}

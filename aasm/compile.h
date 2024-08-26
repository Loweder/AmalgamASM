#ifndef AMALGAM_ASM_COMPILE
#define AMALGAM_ASM_COMPILE

#include "aasm.h"
#include "util.h"

typedef struct {
  const char *(*get_file)(const char *name);
  llist_t *errors;
} cmpl_env_t;

typedef struct {
  const char *section;
  size_t value;
  uint8_t global;
} symbol_t;

typedef struct {
  size_t address;
  size_t size;
  size_t offset;
  char *expression;
} expr_info_t;

typedef struct {
  size_t size;
  llist_t *expr;
  uint8_t *data;
} section_t;

typedef struct {
  hashmap_t *symbols;
  hashmap_t *sections;
} compiled_t;

compiled_t *compile(const char *data, cmpl_env_t *env);
char *link(llist_t *data, cmpl_env_t *env);

#endif

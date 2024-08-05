#ifndef AMALGAM_ASM_COMPILE
#define AMALGAM_ASM_COMPILE

#include "aasm.h"
#include "util.h"

typedef struct {
  const char *(*get_file)(const char *name);
  llist_t *errors;
} cmpl_env;

llist_t *compile(const char *data, cmpl_env *env);
char *link(llist_t *data, cmpl_env *env);

#endif

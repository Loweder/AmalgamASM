#ifndef AMALGAM_ASM_COMPILE
#define AMALGAM_ASM_COMPILE

#include "aasm.h"
#include "util.h"

typedef struct {
  const char *(*get_file)(const char *name);
  char *err;
} cmpl_env;

l_list *preprocess(const char *data, cmpl_env *env);
l_list *compile(l_list *data, cmpl_env *env);
char *link(l_list *data, cmpl_env *env);

#endif

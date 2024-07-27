#include "aasm.h"

typedef struct {
  const char *(*get_file)(const char *name);
  const char *err;
} cmpl_env;

char *preprocess(const char *data, cmpl_env *env);
char *compile(const char *data, cmpl_env *env);
char *link(const char **data, const char *linker, cmpl_env *env);

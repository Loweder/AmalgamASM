#include "aasm/internal/compile.h"

static llist_t *helper_file_substitute(const llist_t *orig, const char *what, const char *with) { //ALLOCATES result[] FULLY.
  llist_t *result = ll_make();
  size_t what_size = strlen(what), with_size = strlen(with);
  int size_diff = (int)(with_size - (what_size+1));
  
  {
    void *file = orig->data->value;
    void *line = orig->data->next->value;
    STRMAKE(strlen(file), new_file, file);
    ll_append(result, new_file);
    ll_append(result, line);
  }
 
  E_FOR(entry, orig->data->next->next) {
    const array_t *o_line = entry->value;
    array_t *line = ar_make(o_line->size);
    ll_append(result, line);
    for (size_t i = 0; i < o_line->size; i++) {
      const char *src = ar_cget(o_line, i);
      size_t size = strlen(src) + 1;
      while(*src) {
	if (*src++ == '\\' && !memcmp(src, what, what_size)) {
	  size += size_diff;
	  src += what_size;
	}
      }
      char *word = emalloc(size);
      ar_set(line, i, word);
      src = ar_cget(o_line, i);
      while(*src) {
	if (*src == '\\' && !memcmp(src + 1, what, what_size)) {
	  memcpy(word, with, with_size);
	  src += what_size+1;
	  word += with_size;
	} else {
	  *word = *src;
	  src++; word++;
	}
      }
      *word = '\0';
    }
  }

  return result;
}
static llist_t *helper_file_copy(const llist_t *orig) {
  llist_t *result = ll_make();
  {
    void *file = orig->data->value;
    void *line = orig->data->next->value;
    STRMAKE(strlen(file), new_file, file);
    ll_append(result, new_file);
    ll_append(result, line);
  }
 
  E_FOR(entry, orig->data->next->next) {
    const array_t *o_line = entry->value;
    array_t *line = ar_make(o_line->size);
    ll_append(result, line);
    for (size_t i = 0; i < o_line->size; i++) {
      STRMAKE(strlen(ar_cget(o_line, i)), word, ar_cget(o_line, i));
      ar_set(line, i, word);
    }
  }

  return result;
}

static void helper_line_add(cmpl_t *_env, llist_t *to) {
  STRMAKE(strlen(_env->file), file, _env->file);
  ll_prepend(to, (void*)_env->f_line);
  ll_prepend(to, file);
}
static void helper_reload(cmpl_t *_env, llist_t *files) {
  llist_t *active = files->data->value;
  helper_line_add(_env, active);
}

void _parse_macro(const array_t *args, cmpl_t *_env, llist_t *dl) {
  ERR_ABOVE(1, ".macro",);
  STRMAKE(strlen(ARG(0)), symbol, ARG(0));
  llist_t *operands = ll_make();
  for (size_t i = 1; i < args->size; i++) {
    STRMAKE(strlen(ARG(i)), operand, ARG(i));
    ll_append(operands, operand);
  }

  helper_line_add(_env, dl);
  ll_prepend(dl, operands);
  ll_prepend(dl, symbol);
}
void _parse_irp(const array_t *args, cmpl_t *_env, llist_t *dl) {
  ERR_ABOVE(1, ".irp",);
  STRMAKE(strlen(ARG(0)), symbol, ARG(0));
  llist_t *operands = ll_make();
  for (size_t i = 1; i < args->size; i++) {
    STRMAKE(strlen(ARG(i)), operand, ARG(i));
    ll_prepend(operands, operand);
  }

  helper_line_add(_env, dl);
  ll_prepend(dl, operands);
  ll_prepend(dl, symbol);
}
void _parse_irpc(const array_t *args, cmpl_t *_env, llist_t *dl) {
  ERR_ONLY(2, ".irpc",);
  STRMAKE(strlen(ARG(0)), symbol, ARG(0));
  STRMAKE(strlen(ARG(1)), operands, ARG(1));

  helper_line_add(_env, dl);
  ll_prepend(dl, operands);
  ll_prepend(dl, symbol);
}
void _parse_include(const array_t *args, cmpl_t *_env, llist_t *files) {
  ERR_ONLY(1, ".include",);

  helper_reload(_env, files);
  ll_prepend(files, helper_file_parse(ARG(0), _env->env));
}
void _parse_ifdef(const array_t *args, cmpl_t *_env, llist_t *history, llist_t *dl) {
  ERR_ONLY(1, ".ifdef",);
  if (hm_get(_env->symbols, ARG(0))) {
    ll_prepend(history, "i1"); 
    helper_line_add(_env, dl);
  } else ll_prepend(history, "i0");
}
void _parse_ifndef(const array_t *args, cmpl_t *_env, llist_t *history, llist_t *dl) {
  ERR_ONLY(1, ".ifndef",);
  if (!hm_get(_env->symbols, ARG(0))) {
    ll_prepend(history, "i1"); 
    helper_line_add(_env, dl);
  } else ll_prepend(history, "i0");
}
void _parse_if(const array_t *args, cmpl_t *_env, llist_t *history, llist_t *dl) {
  ERR_ONLY(1, ".if",);
  if (helper_expr_parse(_env, ARG(0), 0, 0)) {
    ll_prepend(history, "i1"); 
    helper_line_add(_env, dl);
  } else ll_prepend(history, "i0");
}
void _parse_else(cmpl_t *_env, llist_t *history, llist_t *dl) {
  //TODO error if extra args
  if (!strcmp(history->data->value, "i0")) {
    history->data->value = "i1";
    helper_line_add(_env, dl);
  } else if (!strcmp(history->data->value, "i1")) history->data->value = "i0";
}
void _parse_endif(cmpl_t *_env, llist_t *dl, llist_t *files) {
  helper_reload(_env, files);
  if (!dl->size) return;
  ll_prepend(files, dl);
}
void _parse_endm(cmpl_t *_env, llist_t *dl, hashmap_t *macros) {
  const char *label = ll_shift(dl);
  llist_t *old = hm_put(macros, label, dl);
  if (old) ERR("macro already exists",);
}
void _parse_macro_sub(const array_t *args, cmpl_t *_env, llist_t *o_data, llist_t *files) {//USES helper_substitute() TO files[]
  llist_t *operands = ll_shift(o_data);
  llist_t *data = helper_file_copy(o_data);

  size_t i = 0;
  E_FOR(entry, operands->data) {
    char *delim = strchr(entry->value, '=');
    char *what, *with;
    if (delim) {
      STRCPY(delim-((char*)entry->value), what, entry->value);
      with = delim+1;
    } else if (args->size > i) {
      STRCPY(strlen(entry->value), what, entry->value);
      with = (char*) ARG(i);
    } else ERR("Macro call requires more arguments",);
    llist_t *new_data = helper_file_substitute(data, what, with);
    helper_file_free(data);
    data = new_data;
    free(what);
    i++;
  }
  ll_prepend(o_data, operands);
  helper_reload(_env, files);
  ll_prepend(files, data);
}
void _parse_endr(cmpl_t *_env, const char *type, llist_t *dl, llist_t *files) {
  char *symbol = ll_shift(dl);
  helper_reload(_env, files);
  if (!strcmp(type, "rc")) {
    char *operands = ll_shift(dl);
    for (int i = strlen(operands) - 1; i >= 0; i--) {
      uint16_t ch = operands[i];
      ll_prepend(files, helper_file_substitute(dl, symbol, (char*) &ch));
    }
    free(operands);
  } else {
    llist_t *operands = ll_shift(dl);
    E_FOR(operand, operands->data)
      ll_prepend(files, helper_file_substitute(dl, symbol, operand->value));
    ll_free_val(operands);
  }
  free(symbol);
}
void _parse_err(const array_t *args, cmpl_t *_env) { //PURE
  ERR_ONLY(1, ".err",);
  ERR(ARG(0),);
}

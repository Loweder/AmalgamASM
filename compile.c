#include "aasm/compile.h"
#include <stdio.h>
#include <string.h>

#define ARG(x) ((char*)ar_cget(args, x))
#define ERR(code, what) { char *err = emalloc(101); snprintf(err, 100, "%s:%ld: %s", _env->file, _env->f_line, (const char*) code); ll_append(_env->env->errors, err); return what; }
#define LOC_ERR(file, code, what) { char *err = emalloc(101); snprintf(err, 100, "%s:%ld: %s", file, line, (const char*) code); ll_append(env->errors, err); return what; }
#define ERR_BETWEEN(min, max, name, what) if (args->size < min || args->size > max) ERR(name " requires between " #min " and " #max " operands",);
#define ERR_ABOVE(min, name, what) if (args->size < min) ERR(name " requires at least " #min " operand(s)",);
#define ERR_ONLY(value, name, what) if (args->size != value) ERR(name " requires " #value " operand(s)",what);
#define ERR_NONE(name, what) if (args->size != 0) ERR(name " requires no operands",what);
#define STRCPY(size, to, from) to = emalloc((size)+1); memcpy(to, from, size); to[size] = '\0';
#define STRMAKE(size, to, from) char *to = emalloc((size)+1); memcpy(to, from, size); to[size] = '\0';

typedef union _expr_t {
  enum expr_type {
    EXPR_NORM, EXPR_UNARY, EXPR_LIT
  } type;
  struct _expr_norm_t {
    enum expr_type type;
    uint8_t precedence;
    char operand;
    union _expr_t *a;
    union _expr_t *b;
  } norm;
  struct _expr_lit_t {
    enum expr_type type;
    char *literal;
  } lit;
} expr_t;
typedef struct {
  char *file;
  size_t f_line;
  hashmap_t *symbols;
  llist_t *section;
  cmpl_env *env;
} cmpl_t;

static uint8_t _is_space(char datum) {
  return datum == ' ' || datum == '\t' || datum == '\n' || datum == ',' || datum == '\0';
}
//TODO maybe remove
static uint8_t _is_allowed(char datum) {
  return (datum >= '0' && datum <= '9') || (datum >= 'a' && datum <= 'z') || (datum >= 'A' && datum <= 'z') || datum == '_' || datum == '\\';
}
static const char *_word_end(const char *from) {
  const char *invalid;
  if (*from == '"') {
    invalid = strchr(from+1, '"');
    while (invalid && *(invalid-1) == '\\') invalid = strchr(invalid+1, '"');
    if (!invalid) return 0;
    return invalid + 1;
  } else {
    invalid = from;
    while (!_is_space(*invalid)) invalid++;
    return invalid;
  }
}

static llist_t *helper_lex_file(const char *o_name, cmpl_env *env) {
#define LINE_ADD { line++; ll_append(result, r_line); r_line = ll_make(); }
#define LINE_INC { if (*data == '\n') LINE_ADD; data++; }
  llist_t *result = ll_make(), *r_line = ll_make();
  STRMAKE(strlen(o_name), name, o_name)
  const char *o_data = env->get_file(o_name), *data = o_data;
  size_t line = 1;
  ll_append(result, name);
  ll_append(result, (void*)0);
  while(*data) {
    while (_is_space(*data) && *data != '\0') LINE_INC;
    if (!*data) break;
    if (*data == '#' || *data == ';') {
      const char *to = strchr(data, '\n'); 
      if (!to) to = strchr(data, '\0'); 
      data = to;
      continue;
    }

    const char *end = _word_end(data);
    if (!end) LOC_ERR(o_name, "Lexer error: expected \" got EOF",0);

    STRMAKE(end-data, word, data);
    ll_append(r_line, word);

    data = end;
  }
  ll_append(result, r_line);

  free((void*)o_data);
  return result;
#undef LINE_INC
#undef LINE_ADD
}
static llist_t *helper_substitute_file(llist_t *orig, const char *what, const char *with) { //ALLOCATES result[] FULLY.
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
    llist_t *line = ll_make();
    ll_append(result, line);
    E_FOR(w_entry, ((llist_t*)entry->value)->data) {
      const char *src = w_entry->value;
      size_t size = strlen(src) + 1;
      while(*src) {
	if (*src++ == '\\' && !memcmp(src, what, what_size)) {
	  size += size_diff;
	  src += what_size;
	}
      }
      char *word = emalloc(size);
      ll_append(line, word);
      src = w_entry->value;
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
static llist_t *helper_copy_file(llist_t *orig) {
  llist_t *result = ll_make();
  {
    void *file = orig->data->value;
    void *line = orig->data->next->value;
    STRMAKE(strlen(file), new_file, file);
    ll_append(result, new_file);
    ll_append(result, line);
  }
 
  E_FOR(entry, orig->data->next->next) {
    llist_t *line = ll_make();
    ll_append(result, line);
    E_FOR(w_entry, ((llist_t*)entry->value)->data) {
      char *word = emalloc(strlen(w_entry->value)+1);
      ll_append(line, word);
      strcpy(word, w_entry->value);
    }
  }

  return result;
}
static void helper_free_file(llist_t *file) {
  {
    free(ll_shift(file));
    ll_shift(file);
  }
 
  E_FOR(entry, file->data) {
    ll_free_val(entry->value);
  }
  ll_free(file);
}
static void helper_add_line(cmpl_t *_env, llist_t *to) {
  STRMAKE(strlen(_env->file), file, _env->file);
  ll_prepend(to, (void*)_env->f_line);
  ll_prepend(to, file);
}
static void helper_reload(cmpl_t *_env, llist_t *files) {
  llist_t *active = files->data->value;
  helper_add_line(_env, active);
}

static void *helper_add_insn(cmpl_t *_env, size_t size) {
  void *o_mem = emalloc(size);
  llist_t *mem = ll_make();
  ll_append(_env->section, mem);
  ll_append(mem, (void*) size);
  ll_append(mem, o_mem);
  return o_mem;
}

static expr_t *_lex_expr(const char *expr, cmpl_t *_env) {
#define EXPR ((expr_t*)stack->data->value)
#define EXPR_P ((expr_t*)stack->data->next->value)
  llist_t *stack = ll_make();
  ll_prepend(stack, NEW(expr_t));
  while(*expr) {
    switch(*expr) {
      case '!':
      case '-':
      case '~':
	{
	  EXPR->norm.type = EXPR_UNARY;
	  EXPR->norm.operand = *expr;
	  EXPR->norm.a = NEW(expr_t);
	  EXPR->norm.b = 0;
	  EXPR->norm.precedence = 4;
	  ll_prepend(stack, EXPR->norm.a);
	  expr++;
	  break;
	}
      default:
	{
	  EXPR->norm.type = EXPR_NORM;
	  expr_t *left = 0;
	  if (*expr == '(') {
	    expr++;
	    const char *start = expr;
	    uint32_t bracket_level = 1;
	    while(bracket_level && *expr) {
	      if (*expr == '(') bracket_level++;
	      else if (*expr == ')') bracket_level--;
	      expr++;
	    }
	    if (!*expr) {
	      ERR("Expression lexer: no closing brackets", 0);
	    }
	    STRMAKE(expr - start, sub_expr, start);
	    left = _lex_expr(sub_expr, _env);
	    free(sub_expr);
	    expr++;
	  } else {
	    if (!_is_allowed(*expr)) {
	      ERR("Expression lexer: invalid character", 0);
	    }
	    const char *start = expr;
	    while (_is_allowed(*expr)) expr++;
	    STRMAKE(expr - start, literal, start);
	    left = NEW(expr_t);
	    left->lit.type = EXPR_LIT;
	    left->lit.literal = literal;
	  }
	  switch (*expr) {
	    case '*':
	    case '/':
	    case '%':
	      EXPR->norm.precedence = 3;
	      break;
	    case '<':
	      if (*(expr+1) == '<') expr++;
	      EXPR->norm.precedence = 3;
	      break;
	    case '>':
	      if (*(expr+1) == '>') expr++;
	      EXPR->norm.precedence = 3;
	      break;
	    case '&':
	    case '|':
	    case '^':
	      EXPR->norm.precedence = 2;
	      break;
	    case '+':
	    case '-':
	      EXPR->norm.precedence = 1;
	      break;
	    case '\0':
	      free(ll_shift(stack));
	      if (stack->size >= 1) {
		if (EXPR->type == EXPR_UNARY) EXPR->norm.a = left;
		else EXPR->norm.b = left;
	      }
	      ll_prepend(stack, left);
	      expr_t *result = ll_pop(stack);
	      ll_free(stack);
	      return result;
	    default:
	      ERR("Expression lexer: invalid character", 0);
	      break;
	  }
	  EXPR->norm.operand = *expr;
	  if (stack->size == 1 || EXPR->norm.precedence > EXPR_P->norm.precedence) {
	    EXPR->norm.a = left;
	    EXPR->norm.b = NEW(expr_t);
	    ll_prepend(stack, EXPR->norm.b);
	  } else {
	    expr_t *current = ll_pop(stack), *last = 0;
	    if (EXPR->type == EXPR_UNARY) EXPR->norm.a = left;
	    else EXPR->norm.b = left;

	    while (stack->size && current->norm.precedence <= EXPR->norm.precedence) {
	      last = ll_shift(stack);
	    }
	    current->norm.a = last;
	    current->norm.b = NEW(expr_t);
	    ll_prepend(stack, current);
	    ll_prepend(stack, EXPR->norm.b);
	  }
	  expr++;
	  break;
	}
    }
  }
  ERR("Expression lexer: expected expression, got EOL", 0);
#undef EXPR
#undef EXPR_P
}
static size_t _parse_expr(const expr_t *expr, cmpl_t *_env, size_t *callback) {
  switch (expr->type){
    case EXPR_NORM:
      {
	size_t alpha = _parse_expr(expr->norm.a, _env, callback);
	size_t beta = _parse_expr(expr->norm.b, _env, callback);
	if (callback && *callback) return 0;
	switch (expr->norm.operand) {
	  case '*':
	    return alpha * beta;
	  case '/':
	    if (!beta) ERR("Expression parser: division by zero",0);
	    return alpha / beta;
	  case '%':
	    if (!beta) ERR("Expression parser: division by zero",0);
	    return alpha % beta;
	  case '<':
	    return alpha << beta;
	  case '>':
	    return alpha >> beta;
	  case '&':
	    return alpha & beta;
	  case '|':
	    return alpha | beta;
	  case '^':
	    return alpha ^ beta;
	  case '+':
	    return alpha + beta;
	  case '-':
	    return alpha - beta;
	}
	break;
      }
    case EXPR_UNARY: 
      {
	size_t result = _parse_expr(expr->norm.a, _env, callback);
	if (callback && *callback) return 0;
	switch (expr->norm.operand) {
	  case '-':
	    return -result;
	  case '~':
	    return ~result;
	  case '!':
	    return !result;
	}
	break;
      }
    case EXPR_LIT:
      {
	if (!hm_get(_env->symbols, expr->lit.literal))
	  return strtoll(expr->lit.literal, 0, 0);
	llist_t *symbol = hm_get(_env->symbols, expr->lit.literal)->value;
	if (strcmp(symbol->data->value, "absolute")) {
	  if (!callback)
	    ERR("Expression parser: requires absolute operands",0)
	  else *callback = 1;
	} else {
	  return (size_t) symbol->data->next->value;
	}
	break;
      }
  }
  return 0;
}
static void _free_expr(expr_t *expr) {
  switch (expr->type){
    case EXPR_NORM:
      _free_expr(expr->norm.a);
      _free_expr(expr->norm.b);
      break;
    case EXPR_UNARY:
      _free_expr(expr->norm.a);
      break;
    case EXPR_LIT:
      free(expr->lit.literal);
      break;
  }
  free(expr);
}
void helper_symbol_set(cmpl_t *_env, const char *name, size_t value) {
  STRMAKE(strlen(name), symbol, name);
  llist_t *r_symbol = ll_make();
  ll_append(r_symbol, "absolute");
  ll_append(r_symbol, (void*) value);
  llist_t *old = hm_put(_env->symbols, symbol, r_symbol);
  if (old) {
    ll_free(old);
    free(symbol);
  }
}
size_t helper_symbol_expr(cmpl_t *_env, const char *expr, size_t offset, size_t size) {
  expr_t *lexed = _lex_expr(expr, _env);
  if (!lexed) return 0;
  size_t callback = 0;
  size_t result = _parse_expr(lexed, _env, size ? &callback : 0);
  if (callback) {
    llist_t *call = ll_make();
    ll_append(call, (void*) (((size_t)_env->section->data->value) + offset));
    ll_append(call, lexed);
    ll_append(call, (void*) size);
    return 0;
  }
  _free_expr(lexed);
  return result;
}

static void _parse_macro(const array_t *args, cmpl_t *_env, llist_t *dl) {
  ERR_ABOVE(1, ".macro",);
  STRMAKE(strlen(ARG(0)), symbol, ARG(0));
  llist_t *operands = ll_make();
  for (size_t i = 1; i < args->size; i++) {
    STRMAKE(strlen(ARG(i)), operand, ARG(i));
    ll_append(operands, operand);
  }

  helper_add_line(_env, dl);
  ll_prepend(dl, operands);
  ll_prepend(dl, symbol);
}
static void _parse_irp(const array_t *args, cmpl_t *_env, llist_t *dl) {
  ERR_ABOVE(1, ".irp",);
  STRMAKE(strlen(ARG(0)), symbol, ARG(0));
  llist_t *operands = ll_make();
  for (size_t i = 1; i < args->size; i++) {
    STRMAKE(strlen(ARG(i)), operand, ARG(i));
    ll_prepend(operands, operand);
  }

  helper_add_line(_env, dl);
  ll_prepend(dl, operands);
  ll_prepend(dl, symbol);
}
static void _parse_irpc(const array_t *args, cmpl_t *_env, llist_t *dl) {
  ERR_ONLY(2, ".irpc",);
  STRMAKE(strlen(ARG(0)), symbol, ARG(0));
  STRMAKE(strlen(ARG(1)), operands, ARG(1));

  helper_add_line(_env, dl);
  ll_prepend(dl, operands);
  ll_prepend(dl, symbol);
}
static void _parse_include(const array_t *args, cmpl_t *_env, llist_t *files) {
  ERR_ONLY(1, ".include",);

  helper_reload(_env, files);
  ll_prepend(files, helper_lex_file(ARG(0), _env->env));
}
static void _parse_ifdef(const array_t *args, cmpl_t *_env, llist_t *history, llist_t *dl) {
  ERR_ONLY(1, ".ifdef",);
  if (hm_get(_env->symbols, ARG(0))) {
    ll_prepend(history, "i1"); 
    helper_add_line(_env, dl);
  } else ll_prepend(history, "i0");
}
static void _parse_ifndef(const array_t *args, cmpl_t *_env, llist_t *history, llist_t *dl) {
  ERR_ONLY(1, ".ifndef",);
  if (!hm_get(_env->symbols, ARG(0))) {
    ll_prepend(history, "i1"); 
    helper_add_line(_env, dl);
  } else ll_prepend(history, "i0");
}
static void _parse_if(const array_t *args, cmpl_t *_env, llist_t *history, llist_t *dl) {
  ERR_ONLY(1, ".if",);
  if (helper_symbol_expr(_env, ARG(0), 0, 0)) {
    ll_prepend(history, "i1"); 
    helper_add_line(_env, dl);
  } else ll_prepend(history, "i0");
}
static void _parse_else(cmpl_t *_env, llist_t *history, llist_t *dl) {
  //TODO error if extra args
  if (!strcmp(history->data->value, "i0")) {
    history->data->value = "i1";
    helper_add_line(_env, dl);
  } else if (!strcmp(history->data->value, "i1")) history->data->value = "i0";
}
static void _parse_endif(cmpl_t *_env, llist_t *dl, llist_t *files) {
  helper_reload(_env, files);
  if (!dl->size) return;
  ll_prepend(files, dl);
}
static void _parse_endm(cmpl_t *_env, llist_t *dl, hashmap_t *macros) {
  const char *label = ll_shift(dl);
  llist_t *old = hm_put(macros, label, dl);
  if (old) ERR("macro already exists",);
}
static void _parse_macro_sub(const array_t *args, cmpl_t *_env, llist_t *o_data, llist_t *files) {//USES helper_substitute() TO files[]
  llist_t *operands = ll_shift(o_data);
  llist_t *data = helper_copy_file(o_data);

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
    llist_t *new_data = helper_substitute_file(data, what, with);
    helper_free_file(data);
    data = new_data;
    free(what);
    i++;
  }
  ll_prepend(o_data, operands);
  helper_reload(_env, files);
  ll_prepend(files, data);
}
static void _parse_endr(cmpl_t *_env, const char *type, llist_t *dl, llist_t *files) {
  char *symbol = ll_shift(dl);
  helper_reload(_env, files);
  if (!strcmp(type, "rc")) {
    char *operands = ll_shift(dl);
    for (int i = strlen(operands) - 1; i >= 0; i--) {
      uint16_t ch = operands[i];
      ll_prepend(files, helper_substitute_file(dl, symbol, (char*) &ch));
    }
    free(operands);
  } else {
    llist_t *operands = ll_shift(dl);
    E_FOR(operand, operands->data)
      ll_prepend(files, helper_substitute_file(dl, symbol, operand->value));
    ll_free_val(operands);
  }
  free(symbol);
}
static void _parse_err(const array_t *args, cmpl_t *_env) { //PURE
  ERR_ONLY(1, ".err",);
  ERR(ARG(0),);
}
static void _parse_int(const array_t *args, cmpl_t *_env, size_t size) {
  if (args->size > 0) {
    size_t total = args->size*size;
    int8_t *o_mem = helper_add_insn(_env, total);
    for (size_t i = 0; i < args->size; i++) {
      size_t data = helper_symbol_expr(_env, ARG(i), i*size, size);
      memcpy(o_mem+i*size, &data, size);
    }
    *((size_t*)&_env->section->data->value) += total;
  }
}
static void _parse_float(const array_t *args, cmpl_t *_env) { //PURE
  if (args->size > 0) {
    size_t total = args->size*4;
    float *o_mem = helper_add_insn(_env, total);
    for (size_t i = 0; i < args->size; i++) {
      o_mem[i] = strtod(ARG(i), 0);
    }
    *((size_t*)&_env->section->data->value) += total;
  }
}
static void _parse_double(const array_t *args, cmpl_t *_env) { //PURE
  if (args->size > 0) {
    size_t total = args->size*8;
    double *o_mem = helper_add_insn(_env, total);
    for (size_t i = 0; i < args->size; i++) {
      o_mem[i] = strtod(ARG(i), 0);
    }
    *((size_t*)&_env->section->data->value) += total;
  }
}
static void _parse_ascii(const array_t *args, cmpl_t *_env) { //PURE
  if (args->size > 0) {
    size_t total = 0;
    for (int i = 0; i < args->size; i++)
      total += strlen(ARG(i)) - 2;
    char *o_mem = helper_add_insn(_env, total);
    for (size_t i = 0; i < args->size; i++) {
      size_t loc_size = strlen(ARG(i)) - 2;
      memcpy(o_mem, ARG(i)+1, loc_size);
      o_mem += loc_size;
    }
    *((size_t*)&_env->section->data->value) += total;
  }
}
static void _parse_asciz(const array_t *args, cmpl_t *_env) { //PURE
  if (args->size > 0) {
    size_t total = 0;
    for (int i = 0; i < args->size; i++)
      total += strlen(ARG(i)) - 1;
    char *o_mem = helper_add_insn(_env, total);
    for (size_t i = 0; i < args->size; i++) {
      size_t loc_size = strlen(ARG(i)) - 2;
      memcpy(o_mem, ARG(i)+1, loc_size);
      o_mem[loc_size] = '\0';
      o_mem += loc_size + 1;
    }
    *((size_t*)&_env->section->data->value) += total;
  }
}
static void _parse_skip(const array_t *args, cmpl_t *_env) { //PURE
  ERR_BETWEEN(1, 2, ".space/.skip",);
  size_t size = strtol(ARG(0), 0, 0), val = 0;
  if (args->size > 1)
    val = strtol(ARG(1), 0, 0);
  if (!size) ERR(".space/.skip requires a valid size",);
  int8_t *o_mem = helper_add_insn(_env, size);
  memset(o_mem, (int8_t)val, size);
  *((size_t*)&_env->section->data->value) += size;
}
static void _parse_fill(const array_t *args, cmpl_t *_env) { //PURE
  ERR_BETWEEN(1, 3, ".fill",);
  size_t repeat = strtol(ARG(0), 0, 0), size = 0, val = 0;
  if (args->size > 1)
    size = strtol(ARG(1), 0, 0);
  if (args->size > 2)
    val = strtol(ARG(2), 0, 0);
  if(!size) size = 1;
  else if (size > 8) ERR(".fill requires a size less or equal than 8",);
  if (!repeat) ERR(".fill requires a valid repeat",);
  int8_t *o_mem = helper_add_insn(_env, size*repeat);
  for (size_t i = 0; i < size*repeat; i += size) {
    memcpy(o_mem+i, &val, size);
  }
  *((size_t*)&_env->section->data->value) += size*repeat;
}
static void _parse_align(const array_t *args, cmpl_t *_env) { //PURE
  //TODO .balign and .p2align
  ERR_BETWEEN(1, 3, ".align",);
  size_t align = strtol(ARG(0), 0, 0), val = 0, max = 0;
  if (args->size > 1)
    val = strtol(ARG(1), 0, 0);
  if (args->size > 2)
    max = strtol(ARG(2), 0, 0);
  if (!align) ERR(".align requires a valid alignment",);
  size_t *addr_ptr = (size_t*)&_env->section->data->value;
  size_t needed = (align - (*addr_ptr % align)) % align;
  if (needed && (!max || needed < max)) {
    int8_t *o_mem = helper_add_insn(_env, needed);
    memset(o_mem, (int8_t)val, needed);
    *addr_ptr += needed;
  }
}
static void _parse_org(const array_t *args, cmpl_t *_env) { //PURE
  ERR_BETWEEN(1, 2, ".org",);
  size_t until = strtol(ARG(0), 0, 0), val = 0;
  if (args->size > 1)
    val = strtol(ARG(1), 0, 0);
  if (!until) ERR(".org requires a valid address",);
  size_t *addr_ptr = (size_t*)&_env->section->data->value;
  size_t needed = until - *addr_ptr;
  if (until > *addr_ptr) {
    int8_t *o_mem = helper_add_insn(_env, needed);
    memset(o_mem, (int8_t)val, needed);
    *addr_ptr += needed;
  }
}
static void _parse_equ(const array_t *args, cmpl_t *_env) {
  ERR_ONLY(2, ".set/.equ",);
  helper_symbol_set(_env, ARG(0), helper_symbol_expr(_env, ARG(1), 0, 0));
}
static void _parse_equiv(const array_t *args, cmpl_t *_env) {
  ERR_ONLY(2, ".equiv",);
  if (hm_get(_env->symbols, ARG(0)))
    ERR(".equiv failed, already exists",);
  helper_symbol_set(_env, ARG(0), helper_symbol_expr(_env, ARG(1), 0, 0));
}
static void _parse_section(hashmap_t *sections, const char *name, char** s_name) {
  if (hm_get(sections, name)) {
    *s_name = (char*) hm_get(sections, name)->str;
  } else {
    size_t old_32 = (size_t) ((llist_t*)hm_get(sections, name)->value)->data->next->next->value;
    STRCPY(strlen(name), *s_name, name);
    llist_t *section = ll_make();
    hm_put(sections, *s_name, section);
    ll_append(section, 0);
    ll_append(section, ll_make());
    ll_append(section, (void*)old_32);
  }
}

static uint8_t _parse_register(const char *o_word) {
  STRMAKE(strlen(o_word), word, o_word);
  char *d_word = word;
  uint8_t product = 0;
  if (*word == '(' && strchr(word, '\0') - strchr(word, ')') == 1) {
    product |= 0x20; //Is pointer
    word[strlen(word) - 1] = 0;
    word++;
  }
  if (*word != '%') product |= 0xFF; //Not valid
  else word++;

  if (*word == 'e') {
    product |= 0x40; //Extended
    word++;
  }

  if (!strcmp(word, "r1")) product |= 0;
  else if (!strcmp(word, "r2")) product |= 1;
  else if (!strcmp(word, "r3")) product |= 2;
  else if (!strcmp(word, "r4")) product |= 3;
  else if (!strcmp(word, "r5")) product |= 4;
  else if (!strcmp(word, "r6")) product |= 5;
  else if (!strcmp(word, "r7")) product |= 6;
  else if (!strcmp(word, "r8")) product |= 7;
  else if (!strcmp(word, "r9")) product |= 8;
  else if (!strcmp(word, "r10")) product |= 9;
  else if (!strcmp(word, "r11")) product |= 10;
  else if (!strcmp(word, "r12")) product |= 11;
  else if (!strcmp(word, "r13")) product |= 12;
  else if (!strcmp(word, "bp")) product |= 13;
  else if (!strcmp(word, "sp")) product |= 14;
  else if (!strcmp(word, "ip")) product |= 15;
  else product |= 0xFF;

  free(d_word);
  return product;
}
static uint32_t _parse_immediate(const char *word, cmpl_t *_env, size_t offset, size_t size) {
  if (*word == '$') {
    size_t result = helper_symbol_expr(_env, word+1, offset, size);
    return result;
  }
  return 0;
}
static char _parse_type(const char *word) {
  if (strlen(word) < 2) return 'i';
  if (*word == '$')
    return 'l';
  else if ((word[0] == '(' && word[1] == '%') || word[0] == '%')
    return 'r';
  return 'i';
}

static void _parsei_2reg(const array_t *args, cmpl_t *_env, int opcode) {
  ERR_ONLY(2, "Instruction",);
  uint8_t r1 = _parse_register(ARG(0)), r2 = _parse_register(ARG(1));
  if (r1 == 0xFF || r2 == 0xFF) ERR("Operands not valid",);
  if (r1 & 0x20 && r2 & 0x20) ERR("Both operands cannot be pointers",);
  if ((r1 & 0x40) ^ (r2 & 0x40)) ERR("Operands sizes must match",);
  uint8_t *o_mem = emalloc(3);
  llist_t *mem = ll_make();
  ll_append(_env->section, mem);
  ll_append(mem, (void*) 3);
  ll_append(mem, o_mem);
  o_mem[0] = opcode; o_mem[1] = r2 | ((r1 & 0x20) >> 1); o_mem[2] = r1;
  *((size_t*)&_env->section->data->value) += 3;
}
static void _parsei_1reg(const array_t *args, cmpl_t *_env, int opcode) {
  ERR_ONLY(1, "Instruction",);
  uint8_t r1 = _parse_register(ARG(0));
  if (r1 == 0xFF) ERR("Operand not valid",);
  uint8_t *o_mem = emalloc(2);
  llist_t *mem = ll_make();
  ll_append(_env->section, mem);
  ll_append(mem, (void*) 2);
  ll_append(mem, o_mem);
  o_mem[0] = opcode; o_mem[1] = r1;
  *((size_t*)&_env->section->data->value) += 2;
}
static void _parsei_1imm(const array_t *args, cmpl_t *_env, int opcode) {
  ERR_ONLY(1, "Instruction",);
  if (_parse_type(ARG(0)) != 'l') ERR("Operand not valid",);
  uint32_t r1 = _parse_immediate(ARG(0), _env, 1, 2);
  uint8_t *o_mem = emalloc(3);
  llist_t *mem = ll_make();
  ll_append(_env->section, mem);
  ll_append(mem, (void*) 3);
  ll_append(mem, o_mem);
  o_mem[0] = opcode; *(uint16_t*)(o_mem + 1) = r1;
  *((size_t*)&_env->section->data->value) += 3;
}
static void _parsei_2reg_imm(const array_t *args, cmpl_t *_env, int opcode, int alt_opcode) {
  ERR_ONLY(2, "Instruction",);
  uint8_t r_type = _parse_type(ARG(0));
  uint8_t r2 = _parse_register(ARG(1));
  if (r2 == 0xFF) ERR("Operand not valid",);
  if (r_type == 'l') {
    uint32_t imm1 = _parse_immediate(ARG(0), _env, 2, 2);
    if (r2 & 0x40) ERR("Cannot use Extended on 1reg-1imm",);
    uint8_t *o_mem = emalloc(4);
    llist_t *mem = ll_make();
    ll_append(_env->section, mem);
    ll_append(mem, (void*) 4);
    ll_append(mem, o_mem);
    o_mem[0] = alt_opcode; o_mem[1] = r2; ((uint16_t*)o_mem)[1] = imm1;
    *((size_t*)&_env->section->data->value) += 4;
  } else if (r_type == 'r') {
    uint8_t r1 = _parse_register(ARG(0));
    if (r1 & 0x20 && r2 & 0x20) ERR("Both operands cannot be pointers",);
    if ((r1 & 0x40) ^ (r2 & 0x40)) ERR("Operands sizes must match",);
    uint8_t *o_mem = emalloc(3);
    llist_t *mem = ll_make();
    ll_append(_env->section, mem);
    ll_append(mem, (void*) 3);
    ll_append(mem, o_mem);
    o_mem[0] = opcode; o_mem[1] = r2 | ((r1 & 0x20) >> 1); o_mem[2] = r1;
    *((size_t*)&_env->section->data->value) += 3;
  } else ERR("Operand not valid",);
}
static void _parsei_1reg_imm(const array_t *args, cmpl_t *_env, int opcode, int alt_opcode) {
  ERR_ONLY(1, "Instruction",);
  uint8_t r_type = _parse_type(ARG(0));
  if (r_type == 'l') {
    uint32_t r1 = _parse_immediate(ARG(0), _env, 1, 2);
    uint8_t *o_mem = emalloc(3);
    llist_t *mem = ll_make();
    ll_append(_env->section, mem);
    ll_append(mem, (void*) 3);
    ll_append(mem, o_mem);
    o_mem[0] = alt_opcode; *(uint16_t*)(o_mem + 1) = r1;
    *((size_t*)&_env->section->data->value) += 3;
  } else if (r_type == 'r') {
    uint8_t r1 = _parse_register(ARG(0));
    uint8_t *o_mem = emalloc(2);
    llist_t *mem = ll_make();
    ll_append(_env->section, mem);
    ll_append(mem, (void*) 2);
    ll_append(mem, o_mem);
    o_mem[0] = opcode; o_mem[1] = r1;
    *((size_t*)&_env->section->data->value) += 2;
  } else ERR("Operand not valid",);
}
static void _parsei_1reg_1imm(const array_t *args, cmpl_t *_env, int opcode) {
  ERR_ONLY(2, "Instruction",);
  if (_parse_type(ARG(0)) != 'l') ERR("Operands not valid",);
  uint8_t r2 = _parse_register(ARG(1));
  if (r2 == 0xFF) ERR("Operands not valid",);
  uint32_t imm1 = _parse_immediate(ARG(0), _env, 2, 1);
  uint8_t *o_mem = emalloc(3);
  llist_t *mem = ll_make();
  ll_append(_env->section, mem);
  ll_append(mem, (void*) 3);
  ll_append(mem, o_mem);
  o_mem[0] = opcode; o_mem[1] = r2; o_mem[2] = imm1;
  *((size_t*)&_env->section->data->value) += 4;
}
static void _parsei_jcc(const array_t *args, cmpl_t *_env, int opcode, int condition) {
  ERR_ONLY(1, "Instruction",);
  if (_parse_type(ARG(0)) != 'l') ERR("Operand not valid",);
  uint32_t r1 = _parse_immediate(ARG(0), _env, 2, 2);
  uint8_t *o_mem = emalloc(4);
  llist_t *mem = ll_make();
  ll_append(_env->section, mem);
  ll_append(mem, (void*) 4);
  ll_append(mem, o_mem);
  o_mem[0] = opcode; o_mem[1] = condition; ((uint16_t*)o_mem)[1] = r1;
  *((size_t*)&_env->section->data->value) += 4;
}
static void _parsei_movcc(const array_t *args, cmpl_t *_env, int opcode, int condition) {
  ERR_ONLY(2, "Instruction",);
  uint8_t r1 = _parse_register(ARG(0)), r2 = _parse_register(ARG(1));
  if (r1 == 0xFF || r2 == 0xFF) ERR("Operands not valid",);
  if (r1 & 0x20 && r2 & 0x20) ERR("Both operands cannot be pointers",);
  if ((r1 & 0x40) ^ (r2 & 0x40)) ERR("Operands sizes must match",);
  uint8_t *o_mem = emalloc(4);
  llist_t *mem = ll_make();
  ll_append(_env->section, mem);
  ll_append(mem, (void*) 4);
  ll_append(mem, o_mem);
  o_mem[0] = opcode; o_mem[1] = condition; o_mem[2] = r2 | ((r1 & 0x20) >> 1); o_mem[3] = r1;
  *((size_t*)&_env->section->data->value) += 4;
}
static void _parsei_pure(const array_t *args, cmpl_t *_env, int opcode) {
  ERR_NONE("Instruction",);
  uint8_t *o_mem = emalloc(1);
  llist_t *mem = ll_make();
  ll_append(_env->section, mem);
  ll_append(mem, (void*) 1);
  ll_append(mem, o_mem);
  o_mem[0] = opcode;
  *((size_t*)&_env->section->data->value) += 1;
}

//TODO add expressions
////.macro .irp .irpc .ifdef .if .include .err PPT
//.byte .short .int .long .single/.float .double .ascii .asciz CT
//.space/.skip .fill .align .org .code16 .code32 .section CT
//.set/.equ .equiv CT
//.comm .lcomm .globl LT

//Signature convention:
// literal 	= ["*"] , ("word" | "f_line")
// array 	= name , "[" , signature , {", " , signature} , "]"
// set 		= "%" , name , "[" , literal , "]"
// map 		= "#", ["*"] , array
// signature 	= (array | set | map | literal) , ["..."]
// "..." means "0 or more"
// "*" means "needs to be freed". In case of hashmap, key needs to be freed

//Signature args[file, environment] ==> data[#*symbol[address, section], sections[unwrap$[*name, section[size, symbol[address, *name, size]..., *bytecode]]...]]
llist_t *compile(const char *orig, cmpl_env *env) {
  //TODO '.' special symbol
  cmpl_t *const _env = NEW(cmpl_t);
  hashmap_t *const macros = hm_make(), *const sections = hm_make();
  llist_t *const files = ll_make(), *dl = ll_make(), *const history = ll_make();
  STRMAKE(5, s_name, ".text");
  
  {
    _env->env = env;
    _env->file = 0;
    _env->symbols = hm_make();
    _env->section = ll_make();
    ll_append(_env->section, 0);
    ll_append(_env->section, ll_make());
    ll_append(_env->section, (void*)1);
    hm_put(sections, s_name, _env->section);
    ll_append(files, helper_lex_file(orig, env));
  }

skip:
  while (files->data) {
    llist_t *const file = files->data->value;
      
    if (!file) break;
    free(_env->file);
    _env->file = ll_shift(file);
    _env->f_line = (size_t) ll_shift(file);

    while (file->data) {
      llist_t *const line = ll_shift(file);
      _env->f_line++;

      if (!history->size) {
	char *word = ll_shift(line);
	while (word && strchr(word, '\0') - strchr(word, ':') == 1) {
	  STRMAKE(strlen(word) - 1, symbol, word);
	  free(word);
	  if (hm_get(_env->symbols, symbol)) {
	    free(symbol);
	  } else {
	    llist_t *r_symbol = ll_make();
	    ll_append(r_symbol, s_name);
	    ll_append(r_symbol, _env->section->data->value);
	    hm_put(_env->symbols, symbol, r_symbol);
	  }
	  word = ll_shift(line);
	}
	if (!word) {
	  ll_free_val(line);
	  continue;
	}
	array_t *args = ll_free_to(line);
	
	if (*word == '.') {
	  if (!strcmp(word, ".byte")) {
	    _parse_int(args, _env, 1);
	  } else if (!strcmp(word, ".short")) {
	    _parse_int(args, _env, 2);
	  } else if (!strcmp(word, ".int")) {
	    _parse_int(args, _env, 4);
	  } else if (!strcmp(word, ".long")) {
	    _parse_int(args, _env, 8);
	  } else if (!strcmp(word, ".single") || !strcmp(word, ".float")) {
	    _parse_float(args, _env);
	  } else if (!strcmp(word, ".double")) {
	    _parse_double(args, _env);
	  } else if (!strcmp(word, ".ascii")) {
	    _parse_ascii(args, _env);
	  } else if (!strcmp(word, ".asciz")) {
	    _parse_asciz(args, _env);
	  } else if (!strcmp(word, ".include")) {
	    _parse_include(args, _env, files);
	    ar_free_val(args);
	    free(word);
	    goto skip;
	  } else if (!strcmp(word, ".err")) {
	    _parse_err(args, _env);
	  } else if (!strcmp(word, ".ifdef")) {
	    _parse_ifdef(args, _env, history, dl);
	  } else if (!strcmp(word, ".ifndef")) {
	    _parse_ifndef(args, _env, history, dl);
	  } else if (!strcmp(word, ".if")) {
	    _parse_if(args, _env, history, dl);
	  } else if (!strcmp(word, ".macro")) {
	    _parse_macro(args, _env, dl);
	    ll_prepend(history, "m");
	  } else if (!strcmp(word, ".irp")) {
	    _parse_irp(args, _env, dl);
	    ll_prepend(history, "rn");
	  } else if (!strcmp(word, ".irpc")) {
	    _parse_irpc(args, _env, dl);
	    ll_prepend(history, "rc");
	  } else if (!strcmp(word, ".space") || !strcmp(word, ".skip")) {
	    _parse_skip(args, _env);
	  } else if (!strcmp(word, ".fill")) {
	    _parse_fill(args, _env);
	  } else if (!strcmp(word, ".align")) {
	    _parse_align(args, _env);
	  } else if (!strcmp(word, ".org")) {
	    _parse_org(args, _env);
	  } else if (!strcmp(word, ".set") || !strcmp(word, ".equ")) {
	    _parse_equ(args, _env);
	  } else if (!strcmp(word, ".equiv")) {
	    _parse_equiv(args, _env);
	  } else if (!strcmp(word, ".section")) {
	    ERR_ONLY(1, ".section",0);
	    if (*((const char*)ARG(0)) != '.') ERR(".section operand must start from '.'",0);
	    _parse_section(sections, ARG(0), &s_name);
	    _env->section = hm_get(sections, s_name)->value;
	  } else if (!strcmp(word, ".data")) {
	    ERR_NONE(".data",0);
	    _parse_section(sections, ".data", &s_name);
	    _env->section = hm_get(sections, s_name)->value;
	  } else if (!strcmp(word, ".text")) {
	    ERR_NONE(".text",0);
	    _parse_section(sections, ".text", &s_name);
	    _env->section = hm_get(sections, s_name)->value;
	  } else if (!strcmp(word, ".code16")) {
	    ERR_NONE(".code16",0);
	    _env->section->data->next->next->value = 0;
	  } else if (!strcmp(word, ".code32")) {
	    ERR_NONE(".code32",0);
	    _env->section->data->next->next->value = (void*) 1;
	  } else if (!strcmp(word, ".comm")) {

	  } else if (!strcmp(word, ".lcomm")) {

	  } else if (!strcmp(word, ".globl")) {

	  } else {
	    ERR("Unknown directive",0);
	  }
	} else if (strchr(word, '=')) {
	  {
	    char *delim = strchr(word, '=');
	    STRMAKE(delim-word,symbol,word);
	    helper_symbol_set(_env, symbol, helper_symbol_expr(_env, delim+1, 0, 0));
	    free(symbol);
	  }
	} else {
	  if (hm_get(macros, word)) {
	    _parse_macro_sub(args, _env, hm_get(macros, word)->value, files);
	    free(word);
	    ar_free_val(args);
	    goto skip;
	  } else if (!strcmp(word, "nop")) {
	    _parsei_pure(args, _env, I_NOP);
	  } else if (!strcmp(word, "sleep")) {
	    _parsei_1reg_imm(args, _env, I_SLEEP, I_SLEEPI);
	  } else if (!strcmp(word, "gipc")) {
	    _parsei_1reg(args, _env, I_GIPC);
	  } else if (!strcmp(word, "mov")) {
	    _parsei_2reg_imm(args, _env, I_MOV, I_MOVI);
	  } else if (!strcmp(word, "swap")) {
	    _parsei_2reg(args, _env, I_SWAP);
	  } else if (!strcmp(word, "push")) {
	    _parsei_1reg(args, _env, I_PUSH);
	  } else if (!strcmp(word, "pop")) {
	    _parsei_1reg(args, _env, I_POP);
	  } else if (!strcmp(word, "add")) {
	    _parsei_2reg_imm(args, _env, I_ADD, I_ADI);
	  } else if (!strcmp(word, "sub")) {
	    _parsei_2reg_imm(args, _env, I_SUB, I_SBI);
	  } else if (!strcmp(word, "mul")) {
	    _parsei_2reg(args, _env, I_MUL);
	  } else if (!strcmp(word, "div")) {
	    _parsei_2reg(args, _env, I_DIV);
	  } else if (!strcmp(word, "mod")) {
	    _parsei_2reg(args, _env, I_MOD);
	  } else if (!strcmp(word, "inc")) {
	    _parsei_1reg(args, _env, I_INC);
	  } else if (!strcmp(word, "dec")) {
	    _parsei_1reg(args, _env, I_DEC);
	  } else if (!strcmp(word, "fadd")) {
	    _parsei_2reg(args, _env, I_FADD);
	  } else if (!strcmp(word, "fsub")) {
	    _parsei_2reg(args, _env, I_FSUB);
	  } else if (!strcmp(word, "fmul")) {
	    _parsei_2reg(args, _env, I_FMUL);
	  } else if (!strcmp(word, "fdiv")) {
	    _parsei_2reg(args, _env, I_FDIV);
	  } else if (!strcmp(word, "i2f")) {
	    _parsei_2reg(args, _env, I_I2F);
	  } else if (!strcmp(word, "f2i")) {
	    _parsei_2reg(args, _env, I_F2I);
	  } else if (!strcmp(word, "xor")) {
	    _parsei_2reg_imm(args, _env, I_XOR, I_XORI);
	  } else if (!strcmp(word, "or")) {
	    _parsei_2reg_imm(args, _env, I_OR, I_ORI);
	  } else if (!strcmp(word, "and")) {
	    _parsei_2reg_imm(args, _env, I_AND, I_ANDI);
	  } else if (!strcmp(word, "not")) {
	    _parsei_1reg(args, _env, I_NOT);
	  } else if (!strcmp(word, "bts")) {
	    _parsei_1reg_1imm(args, _env, I_BTS);
	  } else if (!strcmp(word, "btr")) {
	    _parsei_1reg_1imm(args, _env, I_BTR);
	  } else if (!strcmp(word, "btc")) {
	    _parsei_1reg_1imm(args, _env, I_BTC);
	  } else if (!strcmp(word, "shl")) {
	    _parsei_1reg_1imm(args, _env, I_SHL);
	  } else if (!strcmp(word, "shr")) {
	    _parsei_1reg_1imm(args, _env, I_SHR);
	  } else if (!strcmp(word, "rol")) {
	    _parsei_1reg_1imm(args, _env, I_ROL);
	  } else if (!strcmp(word, "ror")) {
	    _parsei_1reg_1imm(args, _env, I_ROR);
	  } else if (!strcmp(word, "int")) {
	    _parsei_1imm(args, _env, I_INT);
	  } else if (!strcmp(word, "jmp")) {
	    _parsei_1reg_imm(args, _env, I_JMP, I_JMPI);
	  } else if (!strcmp(word, "call")) {
	    _parsei_1reg_imm(args, _env, I_CALL, I_CALLI);
	  } else if (!strcmp(word, "rjmp")) {
	    _parsei_1reg_imm(args, _env, I_RJMP, I_RJMPI);
	  } else if (!strcmp(word, "rcall")) {
	    _parsei_1reg_imm(args, _env, I_RCALL, I_RCALLI);
	  } else if (!strcmp(word, "jae")) {
	    _parsei_jcc(args, _env, I_JNC, 0x80);
	  } else if (!strcmp(word, "jnae")) {
	    _parsei_jcc(args, _env, I_JC, 0x80);
	  } else if (!strcmp(word, "jb")) {
	    _parsei_jcc(args, _env, I_JC, 0x80);
	  } else if (!strcmp(word, "jnb")) {
	    _parsei_jcc(args, _env, I_JNC, 0x80);
	  } else if (!strcmp(word, "je")) {
	    _parsei_jcc(args, _env, I_JC, 0x82);
	  } else if (!strcmp(word, "jne")) {
	    _parsei_jcc(args, _env, I_JNC, 0x82);
	  } else if (!strcmp(word, "jge")) {
	    _parsei_jcc(args, _env, I_JC, 0x31);
	  } else if (!strcmp(word, "jnge")) {
	    _parsei_jcc(args, _env, I_JNC, 0x31);
	  } else if (!strcmp(word, "jl")) {
	    _parsei_jcc(args, _env, I_JNC, 0x31);
	  } else if (!strcmp(word, "jnl")) {
	    _parsei_jcc(args, _env, I_JC, 0x31);
	  } else if (!strcmp(word, "jc")) {
	    _parsei_jcc(args, _env, I_JC, 0x80);
	  } else if (!strcmp(word, "jnc")) {
	    _parsei_jcc(args, _env, I_JNC, 0x80);
	  } else if (!strcmp(word, "jo")) {
	    _parsei_jcc(args, _env, I_JC, 0x81);
	  } else if (!strcmp(word, "jno")) {
	    _parsei_jcc(args, _env, I_JNC, 0x81);
	  } else if (!strcmp(word, "js")) {
	    _parsei_jcc(args, _env, I_JC, 0x83);
	  } else if (!strcmp(word, "jns")) {
	    _parsei_jcc(args, _env, I_JNC, 0x83);
	  } else if (!strcmp(word, "jz")) {
	    _parsei_jcc(args, _env, I_JC, 0x82);
	  } else if (!strcmp(word, "jnz")) {
	    _parsei_jcc(args, _env, I_JNC, 0x82);
	  } else if (!strcmp(word, "movae")) {
	    _parsei_movcc(args, _env, I_MOVNC, 0x80);
	  } else if (!strcmp(word, "movnae")) {
	    _parsei_movcc(args, _env, I_MOVC, 0x80);
	  } else if (!strcmp(word, "movb")) {
	    _parsei_movcc(args, _env, I_MOVC, 0x80);
	  } else if (!strcmp(word, "movnb")) {
	    _parsei_movcc(args, _env, I_MOVNC, 0x80);
	  } else if (!strcmp(word, "move")) {
	    _parsei_movcc(args, _env, I_MOVC, 0x82);
	  } else if (!strcmp(word, "movne")) {
	    _parsei_movcc(args, _env, I_MOVNC, 0x82);
	  } else if (!strcmp(word, "movge")) {
	    _parsei_movcc(args, _env, I_MOVC, 0x31);
	  } else if (!strcmp(word, "movnge")) {
	    _parsei_movcc(args, _env, I_MOVNC, 0x31);
	  } else if (!strcmp(word, "movl")) {
	    _parsei_movcc(args, _env, I_MOVNC, 0x31);
	  } else if (!strcmp(word, "movnl")) {
	    _parsei_movcc(args, _env, I_MOVC, 0x31);
	  } else if (!strcmp(word, "movc")) {
	    _parsei_movcc(args, _env, I_MOVC, 0x80);
	  } else if (!strcmp(word, "movnc")) {
	    _parsei_movcc(args, _env, I_MOVNC, 0x80);
	  } else if (!strcmp(word, "movo")) {
	    _parsei_movcc(args, _env, I_MOVC, 0x81);
	  } else if (!strcmp(word, "movno")) {
	    _parsei_movcc(args, _env, I_MOVNC, 0x81);
	  } else if (!strcmp(word, "movs")) {
	    _parsei_movcc(args, _env, I_MOVC, 0x83);
	  } else if (!strcmp(word, "movns")) {
	    _parsei_movcc(args, _env, I_MOVNC, 0x83);
	  } else if (!strcmp(word, "movz")) {
	    _parsei_movcc(args, _env, I_MOVC, 0x82);
	  } else if (!strcmp(word, "movnz")) {
	    _parsei_movcc(args, _env, I_MOVNC, 0x82);
	  } else if (!strcmp(word, "ret")) {
	    _parsei_pure(args, _env, I_RET);
	  } else if (!strcmp(word, "in")) {
	    _parsei_2reg_imm(args, _env, I_IN, I_INI);
	  } else if (!strcmp(word, "out")) {
	    _parsei_2reg_imm(args, _env, I_OUT, I_OUTI);
	  } else if (!strcmp(word, "crld")) {
	    _parsei_2reg(args, _env, I_CRLD);
	  } else if (!strcmp(word, "crst")) {
	    _parsei_2reg(args, _env, I_CRST);
	  } else if (!strcmp(word, "sei")) {
	    _parsei_pure(args, _env, I_SEI);
	  } else if (!strcmp(word, "cli")) {
	    _parsei_pure(args, _env, I_CLI);
	  } else if (!strcmp(word, "iret")) {
	    _parsei_pure(args, _env, I_IRET);
	  } else {
	    ERR("Invalid word",0);
	  }
	}
	free(word);
	ar_free_val(args);
      } else {
	char *word = 0;
	E_FOR(entry, line->data) {
	  word = entry->value;
	  if (!entry->value || strchr(entry->value, '\0') - strchr(entry->value, ':') != 1) break;
	}
	if (!word) {
	  ll_append(dl, line);
	  continue;
	}
	
	if ((!strncmp(word, ".if", 3) && ((char*)history->data->value)[0] == 'i') ||
	    (!strcmp(word, ".macro") && ((char*)history->data->value)[0] == 'm') ||
	    (!strncmp(word, ".irp", 4) && ((char*)history->data->value)[0] == 'r')) {
	  ll_prepend(history, history->data->value); // Add frame to history if previous was the same
	} else if (!strcmp(word, ".else") && ((char*)history->data->value)[0] == 'i' && history->size == 1) {
	  _parse_else(_env, history, dl);
	  ll_free_val(line);
	  continue;
	} else if (!strncmp(word, ".end", 4) && ((char*)history->data->value)[0] == word[4] && history->size > 1) {
	  ll_shift(history); // Simply pop frame if there is more than 1 frame
	} else if (!strcmp(word, ".endif") && ((char*)history->data->value)[0] == 'i') {
	  ll_shift(history);
	  _parse_endif(_env, dl, files);
	  dl = ll_make();
	  ll_free_val(line);
	  goto skip;
	} else if (!strcmp(word, ".endm") && ((char*)history->data->value)[0] == 'm') {
	  ll_shift(history);
	  _parse_endm(_env, dl, macros);
	  dl = ll_make();
	  ll_free_val(line);
	  continue;
	} else if (!strcmp(word, ".endr") && ((char*)history->data->value)[0] == 'r') {
	  _parse_endr(_env, ll_shift(history), dl, files);
	  ll_free(dl);
	  dl = ll_make();
	  ll_free_val(line);
	  goto skip;
	}
	ll_append(dl, line);
      }
    }

    ll_shift(files);
    ll_free(file);
  }

  llist_t *result = ll_make();
  {
    llist_t *s_macros = hm_free_to(macros);
    while (s_macros->data) {
      llist_t *entry = ll_shift(s_macros);
      free(ll_shift(entry));
      llist_t *macro = ll_shift(entry);
      ll_free_val(ll_shift(macro));
      helper_free_file(macro);
      ll_free_val(entry);
    }
    ll_free(s_macros);

    llist_t *f_sections = hm_free_to(sections);
    E_FOR(entry, f_sections->data) {
      llist_t *sect = ((llist_t*)entry->value)->data->next->value;
      size_t sect_size = (size_t) ll_shift(sect);
      void* nn1 = ll_shift(sect);
      ll_shift(sect);
      uint8_t *o_product = emalloc(sect_size), *product = o_product;
      while(sect->data) {
	llist_t *part = ll_shift(sect);
	size_t len = (size_t) ll_shift(part);
	uint8_t *data = part->data->value;
	memcpy(product, data, len);
	product += len;
	ll_free_val(part);
      }
      ll_append(sect, (void*)sect_size);
      ll_append(sect, nn1);
      ll_append(sect, o_product);
    }

    ll_free(files);
    ll_free_val(dl);
    ll_free(history);
    ll_append(result, _env->symbols);
    ll_append(result, f_sections);
    free(_env->file);
    free(_env);
  }
  return result;
}

//Signature (data[file[#*symbol[address, section], sections[unwrap$[*name, section[size, symbol[address, *name, size]..., *bytecode]]...]]...])
char *link(llist_t *data, cmpl_env *env) {
  return 0; 
}

#ifndef AMALGAM_ASM_COMPILE_INTERNAL
#define AMALGAM_ASM_COMPILE_INTERNAL

#include "../compile.h"
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
  char *name;
  size_t address;  
  llist_t *expr;
  llist_t *data;
  uint8_t bitness;
} _section_t;
typedef struct {
  char *file;
  size_t f_line;
  hashmap_t *symbols;
  _section_t *section;
  cmpl_env_t *env;
} cmpl_t;

llist_t *helper_file_parse(const char *name, cmpl_env_t *_env);
void helper_file_free(llist_t *file);
size_t helper_expr_parse(cmpl_t *_env, const char *expr, size_t offset, size_t size);
void helper_section_add(hashmap_t *sections, const char *name, _section_t** current);
void helper_symbol_set(cmpl_t *_env, const char *section, const char *name, size_t value);

void _parse_macro(const array_t *args, cmpl_t *_env, llist_t *dl);
void _parse_irp(const array_t *args, cmpl_t *_env, llist_t *dl);
void _parse_irpc(const array_t *args, cmpl_t *_env, llist_t *dl);
void _parse_include(const array_t *args, cmpl_t *_env, llist_t *files);
void _parse_ifdef(const array_t *args, cmpl_t *_env, llist_t *history, llist_t *dl);
void _parse_ifndef(const array_t *args, cmpl_t *_env, llist_t *history, llist_t *dl);
void _parse_if(const array_t *args, cmpl_t *_env, llist_t *history, llist_t *dl);
void _parse_else(cmpl_t *_env, llist_t *history, llist_t *dl);
void _parse_endif(cmpl_t *_env, llist_t *dl, llist_t *files);
void _parse_endm(cmpl_t *_env, llist_t *dl, hashmap_t *macros);
void _parse_macro_sub(const array_t *args, cmpl_t *_env, llist_t *o_data, llist_t *files);
void _parse_endr(cmpl_t *_env, const char *type, llist_t *dl, llist_t *files);
void _parse_err(const array_t *args, cmpl_t *_env);

void _parse_int(const array_t *args, cmpl_t *_env, size_t size);
void _parse_float(const array_t *args, cmpl_t *_env);
void _parse_double(const array_t *args, cmpl_t *_env);
void _parse_ascii(const array_t *args, cmpl_t *_env);
void _parse_asciz(const array_t *args, cmpl_t *_env);
void _parse_skip(const array_t *args, cmpl_t *_env);
void _parse_fill(const array_t *args, cmpl_t *_env);
void _parse_align(const array_t *args, cmpl_t *_env);
void _parse_org(const array_t *args, cmpl_t *_env);
void _parse_equ(const array_t *args, cmpl_t *_env);
void _parse_equiv(const array_t *args, cmpl_t *_env);
void _parse_lcomm(const array_t *args, cmpl_t *_env, hashmap_t *sections, uint8_t global);
void _parse_globl(const array_t *args, cmpl_t *_env);
void _parsei_2reg(const array_t *args, cmpl_t *_env, int opcode);
void _parsei_1reg(const array_t *args, cmpl_t *_env, int opcode);
void _parsei_1imm(const array_t *args, cmpl_t *_env, int opcode);
void _parsei_2reg_imm(const array_t *args, cmpl_t *_env, int opcode, int alt_opcode);
void _parsei_1reg_imm(const array_t *args, cmpl_t *_env, int opcode, int alt_opcode);
void _parsei_1reg_1imm(const array_t *args, cmpl_t *_env, int opcode);
void _parsei_jcc(const array_t *args, cmpl_t *_env, int opcode, int condition);
void _parsei_movcc(const array_t *args, cmpl_t *_env, int opcode, int condition);
void _parsei_pure(const array_t *args, cmpl_t *_env, int opcode);

#endif

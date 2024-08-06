#include "aasm/internal/compile.h"

static void *helper_insn_add(_section_t *section, size_t size) {
  void *o_mem = emalloc(size);
  llist_t *mem = ll_make();
  ll_append(section->data, mem);
  ll_append(mem, (void*) size);
  ll_append(mem, o_mem);
  return o_mem;
}
static void helper_symbol_global(cmpl_t *_env, const char *name) {
  STRMAKE(strlen(name), symbol, name);
  symbol_t *r_symbol = NEW(symbol_t);
  r_symbol->section = 0;
  r_symbol->value = 0;
  r_symbol->global = 1;
  symbol_t *old = hm_put(_env->symbols, symbol, r_symbol);
  if (old) {
    r_symbol->section = old->section;
    r_symbol->value = old->value;
    free(old);
    free(symbol);
  }
}
void helper_symbol_set(cmpl_t *_env, const char *section, const char *name, size_t value) {
  if (!strcmp(name, ".")) {
    size_t *addr_ptr = &_env->section->address;
    size_t needed = value - *addr_ptr;
    if (value > *addr_ptr) {
      int8_t *o_mem = helper_insn_add(_env->section, needed);
      memset(o_mem, 0, needed);
      *addr_ptr += needed;
    }
    return;
  }
  STRMAKE(strlen(name), symbol, name);
  symbol_t *r_symbol = NEW(symbol_t);
  r_symbol->section = section;
  r_symbol->value = value;
  r_symbol->global = 0;
  symbol_t *old = hm_put(_env->symbols, symbol, r_symbol);
  if (old) {
    r_symbol->global = old->global;
    free(old);
    free(symbol);
  }
}

static uint8_t _parse_register(const char *o_word) { //PURE
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
    size_t result = helper_expr_parse(_env, word+1, offset, size);
    return result;
  }
  ERR("Expected immediate, got unknown",0);
}
static char _parse_type(const char *word) { //PURE
  if (strlen(word) < 2) return 'i';
  if (*word == '$')
    return 'l';
  else if ((word[0] == '(' && word[1] == '%') || word[0] == '%')
    return 'r';
  return 'i';
}

void _parse_int(const array_t *args, cmpl_t *_env, size_t size) { //EXPRESSION IMPURITY
  if (args->size > 0) {
    size_t total = args->size*size;
    int8_t *o_mem = helper_insn_add(_env->section, total);
    for (size_t i = 0; i < args->size; i++) {
      size_t data = helper_expr_parse(_env, ARG(i), i*size, size);
      memcpy(o_mem+i*size, &data, size);
    }
    _env->section->address += total;
  }
}
void _parse_float(const array_t *args, cmpl_t *_env) { //PURE
  if (args->size > 0) {
    size_t total = args->size*4;
    float *o_mem = helper_insn_add(_env->section, total);
    for (size_t i = 0; i < args->size; i++) {
      o_mem[i] = strtod(ARG(i), 0);
    }
    _env->section->address += total;
  }
}
void _parse_double(const array_t *args, cmpl_t *_env) { //PURE
  if (args->size > 0) {
    size_t total = args->size*8;
    double *o_mem = helper_insn_add(_env->section, total);
    for (size_t i = 0; i < args->size; i++) {
      o_mem[i] = strtod(ARG(i), 0);
    }
    _env->section->address += total;
  }
}
void _parse_ascii(const array_t *args, cmpl_t *_env) { //PURE
  if (args->size > 0) {
    size_t total = 0;
    for (int i = 0; i < args->size; i++)
      total += strlen(ARG(i)) - 2;
    char *o_mem = helper_insn_add(_env->section, total);
    for (size_t i = 0; i < args->size; i++) {
      size_t loc_size = strlen(ARG(i)) - 2;
      memcpy(o_mem, ARG(i)+1, loc_size);
      o_mem += loc_size;
    }
    _env->section->address += total;
  }
}
void _parse_asciz(const array_t *args, cmpl_t *_env) { //PURE
  if (args->size > 0) {
    size_t total = 0;
    for (int i = 0; i < args->size; i++)
      total += strlen(ARG(i)) - 1;
    char *o_mem = helper_insn_add(_env->section, total);
    for (size_t i = 0; i < args->size; i++) {
      size_t loc_size = strlen(ARG(i)) - 2;
      memcpy(o_mem, ARG(i)+1, loc_size);
      o_mem[loc_size] = '\0';
      o_mem += loc_size + 1;
    }
    _env->section->address += total;
  }
}
void _parse_skip(const array_t *args, cmpl_t *_env) { //PURE
  ERR_BETWEEN(1, 2, ".space/.skip",);
  size_t size = strtol(ARG(0), 0, 0), val = 0;
  if (args->size > 1)
    val = strtol(ARG(1), 0, 0);
  if (!size) ERR(".space/.skip requires a valid size",);
  int8_t *o_mem = helper_insn_add(_env->section, size);
  memset(o_mem, (int8_t)val, size);
  _env->section->address += size;
}
void _parse_fill(const array_t *args, cmpl_t *_env) { //PURE
  ERR_BETWEEN(1, 3, ".fill",);
  size_t repeat = strtol(ARG(0), 0, 0), size = 0, val = 0;
  if (args->size > 1)
    size = strtol(ARG(1), 0, 0);
  if (args->size > 2)
    val = strtol(ARG(2), 0, 0);
  if(!size) size = 1;
  else if (size > 8) ERR(".fill requires a size less or equal than 8",);
  if (!repeat) ERR(".fill requires a valid repeat",);
  int8_t *o_mem = helper_insn_add(_env->section, size*repeat);
  for (size_t i = 0; i < size*repeat; i += size) {
    memcpy(o_mem+i, &val, size);
  }
  _env->section->address += size*repeat;
}
void _parse_align(const array_t *args, cmpl_t *_env) { //PURE
  //TODO .balign and .p2align
  ERR_BETWEEN(1, 3, ".align",);
  size_t align = strtol(ARG(0), 0, 0), val = 0, max = 0;
  if (args->size > 1)
    val = strtol(ARG(1), 0, 0);
  if (args->size > 2)
    max = strtol(ARG(2), 0, 0);
  if (!align) ERR(".align requires a valid alignment",);
  size_t *addr_ptr = &_env->section->address;
  size_t needed = (align - (*addr_ptr % align)) % align;
  if (needed && (!max || needed < max)) {
    int8_t *o_mem = helper_insn_add(_env->section, needed);
    memset(o_mem, (int8_t)val, needed);
    *addr_ptr += needed;
  }
}
void _parse_org(const array_t *args, cmpl_t *_env) { //PURE
  ERR_BETWEEN(1, 2, ".org",);
  size_t until = strtol(ARG(0), 0, 0), val = 0;
  if (args->size > 1)
    val = strtol(ARG(1), 0, 0);
  if (!until) ERR(".org requires a valid address",);
  size_t *addr_ptr = &_env->section->address;
  size_t needed = until - *addr_ptr;
  if (until > *addr_ptr) {
    int8_t *o_mem = helper_insn_add(_env->section, needed);
    memset(o_mem, (int8_t)val, needed);
    *addr_ptr += needed;
  }
}
void _parse_equ(const array_t *args, cmpl_t *_env) { //EXPRESSION IMPURITY
  ERR_ONLY(2, ".set/.equ",);
  helper_symbol_set(_env, "absolute", ARG(0), helper_expr_parse(_env, ARG(1), 0, 0));
}
void _parse_equiv(const array_t *args, cmpl_t *_env) { //EXPRESSION IMPURITY
  ERR_ONLY(2, ".equiv",);
  if (hm_get(_env->symbols, ARG(0)))
    ERR(".equiv failed, already exists",);
  helper_symbol_set(_env, "absolute", ARG(0), helper_expr_parse(_env, ARG(1), 0, 0));
}
void _parse_lcomm(const array_t *args, cmpl_t *_env, hashmap_t *sections, uint8_t global) {
  ERR_ONLY(2,".lcomm/.comm",);
  size_t size = helper_expr_parse(_env, ARG(1), 0, 0);

  _section_t *bss = 0;
  helper_section_add(sections, ".bss", &bss);
  helper_symbol_set(_env, ".bss", ARG(0), bss->address);
  int8_t *o_mem = helper_insn_add(bss, size);
  memset(o_mem, 0, size);
  bss->address += size;

  if (global) helper_symbol_global(_env, ARG(0));
}
void _parse_globl(const array_t *args, cmpl_t *_env) {
  ERR_ONLY(1, ".globl",); 
  helper_symbol_global(_env, ARG(0)); 
}

void _parsei_2reg(const array_t *args, cmpl_t *_env, int opcode) { //PURE
  ERR_ONLY(2, "Instruction",);
  uint8_t r1 = _parse_register(ARG(0)), r2 = _parse_register(ARG(1));
  if (r1 == 0xFF || r2 == 0xFF) ERR("Operands not valid",);
  if (r1 & 0x20 && r2 & 0x20) ERR("Both operands cannot be pointers",);
  if ((r1 & 0x40) ^ (r2 & 0x40)) ERR("Operands sizes must match",);
  uint8_t *o_mem = helper_insn_add(_env->section, 3);
  o_mem[0] = opcode; o_mem[1] = r2 | ((r1 & 0x20) >> 1); o_mem[2] = r1;
  _env->section->address += 3;
}
void _parsei_1reg(const array_t *args, cmpl_t *_env, int opcode) { //PURE
  ERR_ONLY(1, "Instruction",);
  uint8_t r1 = _parse_register(ARG(0));
  if (r1 == 0xFF) ERR("Operand not valid",);
  uint8_t *o_mem = helper_insn_add(_env->section, 2);
  o_mem[0] = opcode; o_mem[1] = r1;
  _env->section->address += 2;
}
void _parsei_1imm(const array_t *args, cmpl_t *_env, int opcode) { //PURE
  ERR_ONLY(1, "Instruction",);
  if (_parse_type(ARG(0)) != 'l') ERR("Operand not valid",);
  uint32_t r1 = _parse_immediate(ARG(0), _env, 1, 2);
  uint8_t *o_mem = helper_insn_add(_env->section, 3);
  o_mem[0] = opcode; *(uint16_t*)(o_mem + 1) = r1;
  _env->section->address += 3;
}
void _parsei_2reg_imm(const array_t *args, cmpl_t *_env, int opcode, int alt_opcode) { //PURE
  ERR_ONLY(2, "Instruction",);
  uint8_t r_type = _parse_type(ARG(0));
  uint8_t r2 = _parse_register(ARG(1));
  if (r2 == 0xFF) ERR("Operand not valid",);
  if (r_type == 'l') {
    uint32_t imm1 = _parse_immediate(ARG(0), _env, 2, 2);
    if (r2 & 0x40) ERR("Cannot use Extended on 1reg-1imm",);
    uint8_t *o_mem = helper_insn_add(_env->section, 4);
    o_mem[0] = alt_opcode; o_mem[1] = r2; ((uint16_t*)o_mem)[1] = imm1;
    _env->section->address += 4;
  } else if (r_type == 'r') {
    uint8_t r1 = _parse_register(ARG(0));
    if (r1 & 0x20 && r2 & 0x20) ERR("Both operands cannot be pointers",);
    if ((r1 & 0x40) ^ (r2 & 0x40)) ERR("Operands sizes must match",);
    uint8_t *o_mem = helper_insn_add(_env->section, 3);
    o_mem[0] = opcode; o_mem[1] = r2 | ((r1 & 0x20) >> 1); o_mem[2] = r1;
    _env->section->address += 3;
  } else ERR("Operand not valid",);
}
void _parsei_1reg_imm(const array_t *args, cmpl_t *_env, int opcode, int alt_opcode) { //PURE
  ERR_ONLY(1, "Instruction",);
  uint8_t r_type = _parse_type(ARG(0));
  if (r_type == 'l') {
    uint32_t r1 = _parse_immediate(ARG(0), _env, 1, 2);
    uint8_t *o_mem = helper_insn_add(_env->section, 3);
    o_mem[0] = alt_opcode; *(uint16_t*)(o_mem + 1) = r1;
    _env->section->address += 3;
  } else if (r_type == 'r') {
    uint8_t r1 = _parse_register(ARG(0));
    uint8_t *o_mem = helper_insn_add(_env->section, 2);
    o_mem[0] = opcode; o_mem[1] = r1;
    _env->section->address += 2;
  } else ERR("Operand not valid",);
}
void _parsei_1reg_1imm(const array_t *args, cmpl_t *_env, int opcode) { //PURE
  ERR_ONLY(2, "Instruction",);
  if (_parse_type(ARG(0)) != 'l') ERR("Operands not valid",);
  uint8_t r2 = _parse_register(ARG(1));
  if (r2 == 0xFF) ERR("Operands not valid",);
  uint32_t imm1 = _parse_immediate(ARG(0), _env, 2, 1);
  uint8_t *o_mem = helper_insn_add(_env->section, 3);
  o_mem[0] = opcode; o_mem[1] = r2; o_mem[2] = imm1;
  _env->section->address += 4;
}
void _parsei_jcc(const array_t *args, cmpl_t *_env, int opcode, int condition) { //PURE
  ERR_ONLY(1, "Instruction",);
  if (_parse_type(ARG(0)) != 'l') ERR("Operand not valid",);
  uint32_t r1 = _parse_immediate(ARG(0), _env, 2, 2);
  uint8_t *o_mem = helper_insn_add(_env->section, 4);
  o_mem[0] = opcode; o_mem[1] = condition; ((uint16_t*)o_mem)[1] = r1;
  _env->section->address += 4;
}
void _parsei_movcc(const array_t *args, cmpl_t *_env, int opcode, int condition) { //PURE
  ERR_ONLY(2, "Instruction",);
  uint8_t r1 = _parse_register(ARG(0)), r2 = _parse_register(ARG(1));
  if (r1 == 0xFF || r2 == 0xFF) ERR("Operands not valid",);
  if (r1 & 0x20 && r2 & 0x20) ERR("Both operands cannot be pointers",);
  if ((r1 & 0x40) ^ (r2 & 0x40)) ERR("Operands sizes must match",);
  uint8_t *o_mem = helper_insn_add(_env->section, 4);
  o_mem[0] = opcode; o_mem[1] = condition; o_mem[2] = r2 | ((r1 & 0x20) >> 1); o_mem[3] = r1;
  _env->section->address += 4;
}
void _parsei_pure(const array_t *args, cmpl_t *_env, int opcode) { //PURE
  ERR_NONE("Instruction",);
  uint8_t *o_mem = helper_insn_add(_env->section, 1);
  o_mem[0] = opcode;
  _env->section->address += 1;
}

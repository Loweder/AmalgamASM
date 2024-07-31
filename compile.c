#include "aasm/compile.h"
#include <stdio.h>
#include <string.h>

#define SIDELOAD_O \
  ll_prepend(files, file);
#define SIDELOAD(our_data) \
  ll_prepend(files, our_data);

#define ADDR_ADD(count) \
  size_t *addr_ptr = (size_t*) &(section->data->value);\
  *addr_ptr += count;
#define ERR(code) { snprintf(env->err, 100, "%s:%ld: %s", f_name, f_line, code); return 0; }

#define OLABEL_CLEAN {\
  char *o_label = ll_shift(r_line);\
  if (o_label && !hs_contains(orphan_symbols, o_label))\
    hs_put(orphan_symbols, o_label);\
  else free(o_label);\
} 

#define T_REGISTER 0
#define T_LITERAL 1
#define T_ILLEGAL 0xFF

static void _debug_print_line(l_list *line) {
  char *file_name = line->data->value;
  size_t line_number = (size_t) line->data->next->value;

  printf("%s:%ld ", file_name, line_number);

  for(struct _le *entry = line->data->next->next; entry; entry = entry->next) {
    printf("%s ", (char*) entry->value);
  }
  printf("\n");
}
static void _debug_print_lines(l_list *lines) { //NOLINT
  for (struct _le *line = lines->data; line; line = line->next) {
    _debug_print_line(line->value);
  }
}

static char *_get_token(char **data, char delim) {
  char *first = strchr(*data, delim);
  if (!first) {
    size_t size = strlen(*data);
    char *result = emalloc(size + 1);
    memcpy(result, *data, size + 1);
    *data += size;
    return result;
  }
  size_t size = first - *data;
  char *result = emalloc(size + 1);
  memcpy(result, *data, size);
  result[size] = '\0';
  *data = first + 1;
  return result;
}

static uint8_t _s_with(const char *data, char sym) {
  return *data == sym;
}
static uint8_t _e_with(const char *data, char sym) {
  char *inter = strchr(data, sym);
  return inter && !*(inter + 1);
}
static uint8_t _is_space(char datum) {
  return datum == ' ' || datum == '\t' || datum == '\0';
}
static uint8_t _is_s_space(char datum) {
  return _is_space(datum) || datum == ',';
}

static void _clear_ws(char **data) {
  uint8_t flag = 1, par = 0;
  size_t size = 1;
  for (char *src = *data; *src; src++) {
    if (_is_space(*src)) {
      if (!flag || par) {flag = 1; size++; }
    } else {
      if (!par && flag && *src == '"') par = 1;
      else if (par && *src == '"' && *(src-1) != '\\') par = !_is_s_space(*(src+1));
      if (*src == ',' || *src == ':') flag = 1;
      else flag = 0;
      size += (*src == ':') ? 2 : 1;
    }
  }
  if (flag && size > 1) size--;
  char *o_dest = NEW_A(char, size), *dest = o_dest;
  flag = 1, par = 0;

  for(char *src = *data; *src; src++) {
    if (_is_space(*src)) {
      if (par) { flag = 1; *dest++ = *src; }
      else if (!flag) { flag = 1; *dest++ = ' '; }
    } else {
      if (!par && flag && *src == '"') par = 1;
      else if (par && *src == '"' && *(src-1) != '\\') par = !_is_s_space(*(src+1));
      if (*src != ',' || par) *dest++ = *src;
      if ((!par && *src == ',') || *src == ':') { flag = 1; *dest++ = ' '; }
      else flag = 0;
    } 
  }
  if (flag && size > 1) dest--;
  *dest = '\0';
  free(*data);
  *data = o_dest;
}

static l_list *_copy_pos(l_list *line) {
  l_list *result = ll_make();
  
  char *f_name = emalloc(strlen(line->data->value) + 1);
  strcpy(f_name, line->data->value);
  ll_append(result, f_name);
  ll_append(result, line->data->next->value);

  return result;
}
static l_list *_copy_line(l_list *line) {
  l_list *result = _copy_pos(line);

  for (struct _le *entry = line->data->next->next; entry; entry = entry->next) {
    if (!entry->value) {
      ll_append(result, 0);
      continue;
    }
    char *word = emalloc(strlen(entry->value) + 1);
    strcpy(word, entry->value);
    ll_append(result, word);
  }
  return result;
}

static l_list *_ll_from(const char *o_data, char delim) {
  const char *data = o_data;
  l_list *result = ll_make();
  while(*data) {
    ll_append(result, _get_token((char**)&data, delim));
  }
  free((void*)o_data);
  return result;
}
static l_list *_ll_sub(l_list *orig, const char *what, const char *with) {
  l_list *result = ll_make();
  size_t what_size = strlen(what), with_size = strlen(with);
  int size_diff = (int)(with_size - (what_size+1));
  for (struct _le *entry = orig->data; entry; entry = entry->next) {
    l_list *o_line = entry->value;
    l_list *r_line = _copy_pos(o_line);
    ll_append(result, r_line);
    for (struct _le *n_entry = o_line->data->next->next; n_entry; n_entry = n_entry->next) {
      char *o_word = n_entry->value;
      if (!o_word) {
	ll_append(r_line, 0);
	continue;
      }
      size_t size = strlen(o_word) + 1;
      const char *src = o_word;
      while(*src) {
	if (*src++ == '\\' && !memcmp(src, what, what_size)) {
	  size += size_diff;
	  src += what_size;
	}
      }
      char *r_word = emalloc(size);
      ll_append(r_line, r_word);
      src = o_word;

      while(*src) {
	if (*src == '\\' && !memcmp(src + 1, what, what_size)) {
	  memcpy(r_word, with, with_size);
	  src += what_size+1;
	  r_word += with_size;
	} else {
	  *r_word = *src;
	  src++; r_word++;
	}
      }
      *r_word = '\0';
    }
  }
  return result;
}

static l_list *_pp_format(const char *o_data, const char *name) {
  size_t f_line = 0;
  l_list *result = ll_make(), *data = _ll_from(o_data, '\n');

  while(data->data) {
    char *o_line = ll_shift(data);
    _clear_ws(&o_line);
    l_list *line = _ll_from(o_line, ' '), *r_line = ll_make();
    char *c_name = emalloc(strlen(name) + 1);
    strcpy(c_name, name);
    ll_append(r_line, c_name);
    ll_append(r_line, (void*) ++f_line);
    while(line->data) {
      char *word = ll_shift(line);
      if (_s_with(word, '"')) {
	size_t total_len = strlen(word);
	char *result = emalloc(total_len + 1);
	strcpy(result, word);
	free(word);

	while (1) {
	  if (!line->data) break;
	  word = ll_shift(line);

	  size_t word_len = strlen(word);
	  char *new_result = emalloc(total_len + word_len + 2);
	  memcpy(new_result, result, total_len);
	  memcpy(new_result + total_len + 1, word, word_len);
	  new_result[total_len + word_len + 1] = '\0';
	  new_result[total_len] = ' ';
	  free(result);
	  result = new_result;
	  if (_e_with(word, '"')) {
	    free(word);
	    break;
	  } else {
	    free(word);
	    total_len += word_len + 1;
	  }
	}
	word = result;
      }

      if (_s_with(word, '#') || _s_with(word, ';')) {
	free(word);
	break;
      }
      if (r_line->size == 2) {
	if (_e_with(word, ':')) {
	  ll_append(r_line, word);
	  word[strlen(word) - 1] = '\0';
	  continue;
	}
	ll_append(r_line, 0);
      }
      ll_append(r_line, word);
    }
    ll_free_val(line);
    if (r_line->size < 3)
      ll_free(r_line);
    else
      ll_append(result, r_line);
  }
  
  ll_free(data);
  return result;
}
static const char *_pp_macro_sub(l_list *line, l_list *o_sl_data, l_list *files) { //0-skip, err
  l_list *sl_data = ll_make();
  l_list *o_operands = ll_shift(o_sl_data), *operands = ll_make();

  for (struct _le *entry = o_sl_data->data; entry; entry = entry->next) {
    l_list *entry_str = _copy_line(entry->value);
    ll_append(sl_data, entry_str);
  }
  for (struct _le *entry = o_operands->data; entry; entry = entry->next) {
    char *entry_str = emalloc(strlen(entry->value) + 1);
    strcpy(entry_str, entry->value);
    ll_append(operands, entry_str);
  }
  ll_prepend(o_sl_data, o_operands);

  struct _le *c_entry = line->data;
  while (operands->data) {
    l_list *se = _ll_from(ll_shift(operands), '=');
    const char *what = se->data->value, *with;
    if (se->size > 2) return ".macro operand must be 'a=c' or 'a'";
    else if (c_entry) { with = c_entry->value; c_entry = c_entry->next; }
    else if (se->size == 2) with = se->data->next->value;
    else return "Macro call requires more arguments";
    l_list *t_sl_data = _ll_sub(sl_data, what, with);
    ll_free_val(sl_data);
    sl_data = t_sl_data;
    ll_free_val(se);
  }
  SIDELOAD(sl_data);
  ll_free_val(operands);

  //TODO currently label on macros is ignored, maybe put it to a new line
  return 0;
}
static const char *_pp_parse_set(l_list *line, l_list *result, l_list *history, hashset *symbols) { //01-fskip, err
  if (line->size != 2) return ".set/.equ/.equiv require 2 operands";
  if (!history->data) {
    if (!hs_contains(symbols, line->data->value))
      hs_put(symbols, line->data->value);
  }
  return (char*) (size_t) (history->data && !strcmp(history->data->value, "i0"));
}
static const char *_pp_parse_err(l_list *line, l_list *result, l_list *history) { //0, err
  if (line->size < 1) return ".err requires label";
  if (!history->data)
    return ll_shift(line);
  return 0;
}
static const char *_pp_parse_include(l_list *line, l_list *result, l_list *history, cmpl_env *env, l_list *files) { //0, 1-skip, err
  if (line->size != 1) return ".include requires 1 operand";
  if (history->data) return 0;
  l_list *sl_data = _pp_format(env->get_file(line->data->value), line->data->value);
  SIDELOAD(sl_data);
  return (void*) 1;
}
static const char *_pp_parse_macro(l_list *line, l_list *result, l_list *history) { //0, err
  if (line->size < 1) return ".macro requires label";
  if (history->data) return 0;
  ll_append(result, ll_shift(line));
  l_list *operands = ll_make();
  while (line->data) ll_append(operands, ll_shift(line));
  ll_append(result, operands);
  return 0;
}
static const char *_pp_parse_endm(l_list *line, l_list **result, l_list *history, hashmap *macros) { //0, err
  if (line->size != 0) return ".endm requires no operands";
  if (!history->data) return ".endm requires .macro";
  ll_shift(history);
  if (history->data) return 0;
  const char *o_label = ll_shift(*result);
  hm_put(macros, o_label, *result);
  *result = ll_make();
  return 0;
}
static const char *_pp_parse_irp(l_list *line, l_list *result, l_list *history) { //0, err
  if (line->size < 1) return ".irp requires label";
  if (history->data) return 0;
  ll_append(result, ll_shift(line));
  l_list *operands = ll_make();
  while (line->data) ll_prepend(operands, ll_shift(line));
  ll_append(result, operands);
  return 0;
}
static const char *_pp_parse_irpc(l_list *line, l_list *result, l_list *history) { //0, err
  if (line->size != 2) return ".irpc requires 2 operands";
  if (history->data) return 0;
  ll_append(result, ll_shift(line));
  ll_append(result, ll_shift(line));
  return 0;
}
static const char *_pp_parse_endr(l_list *line, l_list **result, l_list *history, l_list *files) { //0, 1-skip, err
  if (line->size != 0) return ".endr requires no operands";
  if (!history->data) return ".endr requires .irp or .irpc";
  const char *type = ll_shift(history);
  if (history->data) return 0;
  const char *o_label = ll_shift(*result);
  l_list *sl_data;
  if (!strcmp(type, "rc")) {
    const char *o_operands = ll_shift(*result);
    for (int i = strlen(o_operands) - 1; i >= 0; i--) {
      uint16_t ch = o_operands[i];
      sl_data = _ll_sub(*result, o_label, (char*) &ch);
      SIDELOAD(sl_data);
    }
    free((void*)o_operands);
  } else {
    l_list *o_operands = ll_shift(*result);
    for (struct _le *operand = o_operands->data; operand; operand = operand->next) {
      sl_data = _ll_sub(*result, o_label, operand->value);
      SIDELOAD(sl_data);
    }
    ll_free_val(o_operands);
  }
  ll_free_val(*result);
  free((void*)o_label);
  *result = ll_make();
  return (void*) 1;
}
static const char *_pp_parse_ifdef(l_list *line, l_list *result, l_list *history, hashset *symbols) { //0, err
  if (line->size != 1) return ".ifdef requires 1 operand";
  if (!history->data) {
    if (hs_contains(symbols, line->data->value)) ll_prepend(history, "i1");  
    else ll_prepend(history, "i0");
  } else {
    if (!strcmp(history->data->value, "i0")) ll_prepend(history, "i0");
    else if (!strcmp(history->data->value, "i1")) ll_prepend(history, "i1");
  }
  return 0;
}
static const char *_pp_parse_ifndef(l_list *line, l_list *result, l_list *history, hashset *symbols) { //0, err
  if (line->size != 1) return ".ifndef requires 1 operand";
  if (!history->data) {
    if (hs_contains(symbols, line->data->value)) ll_prepend(history, "i0");  
    else ll_prepend(history, "i1");
  } else {
    if (!strcmp(history->data->value, "i0")) ll_prepend(history, "i0");
    else if (!strcmp(history->data->value, "i1")) ll_prepend(history, "i1");
  }
  return 0;
}
static const char *_pp_parse_else(l_list *line, l_list *result, l_list *history) { //01-fskip, err
  if (line->size != 0) return ".else requires no operands";
  if (!history->size) return ".else requires .ifdef or .ifndef";
  if (history->size > 1) return 0;
  if (!strcmp(history->data->value, "i0")) history->data->value = "i1";
  else if (!strcmp(history->data->value, "i1")) history->data->value = "i0";
  return (void*) 1;
}
static const char *_pp_parse_endif(l_list *line, l_list **result, l_list *history, l_list *files) { //0, 1-skip, err
  if (line->size != 0) return ".endif requires no operands";
  if (!history->data) return ".endif requires .ifdef or .ifndef";
  ll_shift(history);
  if (history->data) return 0;
  SIDELOAD(*result);
  *result = ll_make();
  return (void*) 1;
}

//TODO add expressions
static void _cl_parse_int(l_list *line, l_list *section, size_t size) {
  if (line->size > 0) {
    int8_t *o_mem = emalloc(line->size*size);
    l_list *mem = ll_make();
    ADDR_ADD(line->size*size);
    ll_append(section, mem);
    ll_append(mem, (void*) (line->size*size));
    ll_append(mem, o_mem);
    while (line->data) {
      char *operand = ll_shift(line);
      long long int i_operand = strtoll(operand, 0, 0);
      memcpy(o_mem, &i_operand, size);
      o_mem += size;
      free(operand);
    }
  }
}
static void _cl_parse_float(l_list *line, l_list *section) {
  if (line->size > 0) {
    float *o_mem = emalloc(line->size*4);
    l_list *mem = ll_make();
    ADDR_ADD(line->size*4);
    ll_append(section, mem);
    ll_append(mem, (void*) (line->size*4));
    ll_append(mem, o_mem);
    while (line->data) {
      char *operand = ll_shift(line);
      float i_operand = strtod(operand, 0);
      *o_mem++ = i_operand;
      free(operand);
    }
  }
}
static void _cl_parse_double(l_list *line, l_list *section) {
  if (line->size > 0) {
    double *o_mem = emalloc(line->size*8);
    l_list *mem = ll_make();
    ADDR_ADD(line->size*8);
    ll_append(section, mem);
    ll_append(mem, (void*) (line->size*8));
    ll_append(mem, o_mem);
    while (line->data) {
      char *operand = ll_shift(line);
      double i_operand = strtod(operand, 0);
      *o_mem++ = i_operand;
      free(operand);
    }
  }
}
static void _cl_parse_ascii(l_list *line, l_list *section) {
  if (line->size > 0) {
    size_t calc_size = 0;
    for (struct _le *entry = line->data; entry; entry = entry->next)
      calc_size += strlen(entry->value) - 2;
    char *o_mem = emalloc(calc_size);
    l_list *mem = ll_make();
    ADDR_ADD(calc_size);
    ll_append(section, mem);
    ll_append(mem, (void*) calc_size);
    ll_append(mem, o_mem);
    while (line->data) {
      char *operand = ll_shift(line);
      size_t loc_size = strlen(operand) - 2;
      memcpy(o_mem, operand+1, loc_size);
      o_mem += loc_size;
      free(operand);
    }
  }
}
static void _cl_parse_asciz(l_list *line, l_list *section) {
  if (line->size > 0) {
    size_t calc_size = 0;
    for (struct _le *entry = line->data; entry; entry = entry->next)
      calc_size += strlen(entry->value) - 1;
    char *o_mem = emalloc(calc_size);
    l_list *mem = ll_make();
    ADDR_ADD(calc_size);
    ll_append(section, mem);
    ll_append(mem, (void*) calc_size);
    ll_append(mem, o_mem);
    while (line->data) {
      char *operand = ll_shift(line);
      size_t loc_size = strlen(operand) - 2;
      memcpy(o_mem, operand+1, loc_size);
      o_mem[loc_size] = '\0';
      o_mem += loc_size;
      free(operand);
    }
  }
}
static const char *_cl_parse_skip(l_list *line, l_list *section) {
  if (line->size > 2 || line->size < 1) return ".space/.skip requires 2 or 1 operands";
  char *o_size = ll_shift(line);
  size_t size = strtol(o_size, 0, 0), val = 0;
  free(o_size);
  if (line->data) {
    char *o_val = ll_shift(line);
    val = strtol(o_val, 0, 0);
    free(o_val);
  }
  if (!size) return ".space/.skip requires a valid size";
  int8_t *o_mem = emalloc(size);
  l_list *mem = ll_make();
  ADDR_ADD(size);
  ll_append(section, mem);
  ll_append(mem, (void*) size);
  ll_append(mem, o_mem);
  memset(o_mem, (int8_t)val, size);
  return 0;
}
static const char *_cl_parse_fill(l_list *line, l_list *section) {
  if (line->size > 3 || line->size < 1) return ".fill requires between 3 and 1 operands";
  char *o_repeat = ll_shift(line);
  size_t repeat = strtol(o_repeat, 0, 0), size = 1, val = 0;
  free(o_repeat);
  if (line->data) {
    char *o_size = ll_shift(line);
    size = strtol(o_size, 0, 0);
    free(o_size);
    if(!size) size = 1;
    else if (size > 8) return ".fill requires a size less or equal than 8";
  }
  if (line->data) {
    char *o_val = ll_shift(line);
    val = strtol(o_val, 0, 0);
    free(o_val);
  }
  if (!repeat) return ".fill requires a valid repeat";
  int8_t *o_mem = emalloc(size*repeat);
  l_list *mem = ll_make();
  ADDR_ADD(size*repeat);
  ll_append(section, mem);
  ll_append(mem, (void*) (size*repeat));
  ll_append(mem, o_mem);
  for (size_t i = 0; i < size*repeat; i += size) {
    memcpy(o_mem+i, &val, size);
  }
  return 0;
}
static const char *_cl_parse_align(l_list *line, l_list *section) {
  //TODO .balign and .p2align
  if (line->size > 3 || line->size < 1) return ".align requires between 3 and 1 operands";
  char *o_align = ll_shift(line);
  size_t align = strtol(o_align, 0, 0), val = 0, max = 0;
  free(o_align);
  if (line->data) {
    char *o_val = ll_shift(line);
    val = strtol(o_val, 0, 0);
    free(o_val);
  }
  if (line->data) {
    char *o_max = ll_shift(line);
    max = strtol(o_max, 0, 0);
    free(o_max);
  }
  if (!align) return ".align requires a valid alignment";
  size_t *addr_ptr = (size_t*)&(section->data->value);
  size_t needed = (align - (*addr_ptr % align)) % align;
  if (needed && (!max || needed < max)) {
    int8_t *o_mem = emalloc(needed);
    l_list *mem = ll_make();
    *addr_ptr += needed;
    ll_append(section, mem);
    ll_append(mem, (void*) needed);
    ll_append(mem, o_mem);
    memset(o_mem, (int8_t)val, needed);
  }
  return 0;
}
static const char *_cl_parse_org(l_list *line, l_list *section) {
  if (line->size > 2 || line->size < 1) return ".org requires 2 or 1 operands";
  char *o_until = ll_shift(line);
  size_t until = strtol(o_until, 0, 0), val = 0;
  free(o_until);
  if (line->data) {
    char *o_val = ll_shift(line);
    val = strtol(o_val, 0, 0);
    free(o_val);
  }
  if (!until) return ".org requires a valid address";
  size_t *addr_ptr = (size_t*)&(section->data->value);
  if (until > *addr_ptr) {
    size_t needed = until - *addr_ptr;
    int8_t *o_mem = emalloc(needed);
    l_list *mem = ll_make();
    *addr_ptr += needed;
    ll_append(section, mem);
    ll_append(mem, (void*) needed);
    ll_append(mem, o_mem);
    memset(o_mem, (int8_t)val, needed);
  }
  return 0;
}
static const char *_cl_parse_equ(l_list *line, l_list *section, hashmap *equs) {
  if (line->size != 2) return ".set/.equ requires 2 operands";
  char *name = ll_shift(line);
  char *value = ll_shift(line);
  long long int true_value = strtoll(value, 0, 0);
  l_list *r_value = ll_make();
  ll_append(r_value, (void*) true_value);
  ll_append(r_value, ".absolute");
  l_list *old = hm_put(equs, name, r_value);
  if (old) {
    ll_free(old);
    free(name);
  }
  free(value);
  return 0;
}
static const char *_cl_parse_equiv(l_list *line, l_list *section, hashmap *equs) {
  if (line->size != 2) return ".equiv requires 2 operands";
  char *name = ll_shift(line);
  char *value = ll_shift(line);
  long long int true_value = strtoll(value, 0, 0);
  if (hm_contains(equs, name))
    return ".equiv failed, already exists";
  l_list *r_value = ll_make();
  ll_append(r_value, (void*) true_value);
  ll_append(r_value, ".rodata");
  hm_put(equs, name, r_value);
  free(value);
  return 0;
}
static void _cl_parse_section(hashmap *sections, char *name, l_list **section, char **s_name) {
  if (hm_contains(sections, name)) {
    *s_name = (char*) hm_get(sections, name)->str;
    *section = hm_get(sections, name)->value;
  } else {
    *s_name = emalloc(strlen(name) + 1);
    strcpy(*s_name, name);
    size_t old_32 = (size_t) (*section)->data->next->value;
    *section = ll_make();
    hm_put(sections, *s_name, *section);
    ll_append(*section, 0);
    ll_append(*section, (void*)old_32);
    ll_append(*section, ll_make());
  }
}

static uint8_t _cl_parse_register(char *word) {
  uint8_t product = 0;
  if (_s_with(word, '(') && _e_with(word, ')')) {
    product |= 0x20; //Is pointer
    word[strlen(word) - 1] = 0;
    word++;
  }
  if (!_s_with(word, '%')) return 0xFF; //Not valid
  else word++;

  if (_s_with(word, 'e')) {
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
  else return 0xFF;

  return product;
}
static uint32_t _cl_parse_immediate(char *word, l_list *section, size_t offset, size_t size) {
  if (_s_with(word, '$')) {
    if (word[1] >= '0' && word[1] <= '9') 
      return strtol(word+1, 0, 0);
    l_list *symbol_sub = ll_make();
    size_t len = strlen(word);
    char *name = emalloc(len);
    strcpy(name, word+1);
    ll_append(symbol_sub, ((uint8_t*)section->data->value) + offset);
    ll_append(symbol_sub, name);
    ll_append(symbol_sub, (void*)size);
    ll_append(section->data->next->next->value, symbol_sub);
    return 0;
  } else {
    return 0;
  }
}
static uint8_t _cl_parse_type(char *word) {
  if (strlen(word) < 2) return T_ILLEGAL;
  if (_s_with(word, '$'))
    return T_LITERAL;
  else if ((word[0] == '(' && word[1] == '%') || word[0] == '%')
    return T_REGISTER;
  return T_ILLEGAL;
}

static const char *_cl_parsei_2reg(l_list *line, l_list *section, int opcode) {
  if (line->size != 2) return "Needs 2 operands"; //TODO descriptive error 
  char *r1_raw = ll_shift(line), *r2_raw = ll_shift(line);
  uint8_t r1 = _cl_parse_register(r1_raw), r2 = _cl_parse_register(r2_raw);
  if (r1 == 0xFF || r2 == 0xFF) return "Operands not valid";
  if (r1 & 0x20 && r2 & 0x20) return "Both operands cannot be pointers";
  if ((r1 & 0x40) ^ (r2 & 0x40)) return "Operands sizes must match";
  uint8_t *o_mem = emalloc(3);
  l_list *mem = ll_make();
  ADDR_ADD(3);
  ll_append(section, mem);
  ll_append(mem, (void*) 3);
  ll_append(mem, o_mem);
  o_mem[0] = opcode; o_mem[1] = r2 | ((r1 & 0x20) >> 1); o_mem[2] = r1;
  free(r1_raw);
  free(r2_raw);
  return 0;
}
static const char *_cl_parsei_1reg(l_list *line, l_list *section, int opcode) {
  if (line->size != 1) return "Needs 1 operand"; //TODO descriptive error 
  char *r1_raw = ll_shift(line);
  uint8_t r1 = _cl_parse_register(r1_raw);
  if (r1 == 0xFF) return "Operand not valid";
  uint8_t *o_mem = emalloc(2);
  l_list *mem = ll_make();
  ADDR_ADD(2);
  ll_append(section, mem);
  ll_append(mem, (void*) 2);
  ll_append(mem, o_mem);
  o_mem[0] = opcode; o_mem[1] = r1;
  free(r1_raw);
  return 0;
}
static const char *_cl_parsei_1imm(l_list *line, l_list *section, int opcode) {
  if (line->size != 1) return "Needs 1 operand"; //TODO descriptive error 
  char *r1_raw = ll_shift(line);
  if (_cl_parse_type(r1_raw) != T_LITERAL) return "Operand not valid";
  uint32_t r1 = _cl_parse_immediate(r1_raw, section, 1, 2);
  uint8_t *o_mem = emalloc(3);
  l_list *mem = ll_make();
  ADDR_ADD(3);
  ll_append(section, mem);
  ll_append(mem, (void*) 3);
  ll_append(mem, o_mem);
  o_mem[0] = opcode; *((uint16_t*)o_mem + 1) = r1;
  free(r1_raw);
  return 0;
}
static const char *_cl_parsei_2reg_imm(l_list *line, l_list *section, int opcode, int alt_opcode) {
  if (line->size != 2) return "Needs 2 operands"; //TODO descriptive error 
  char *r1_raw = ll_shift(line), *r2_raw = ll_shift(line);
  uint8_t r_type = _cl_parse_type(r1_raw);
  uint8_t r2 = _cl_parse_register(r2_raw);
  if (r2 == 0xFF) return "Operand not valid";
  if (r_type == T_LITERAL) {
    uint32_t imm1 = _cl_parse_immediate(r1_raw, section, 2, 2);
    if (r2 & 0x40) return "Cannot use Extended on 1reg-1imm";
    uint8_t *o_mem = emalloc(4);
    l_list *mem = ll_make();
    ADDR_ADD(4);
    ll_append(section, mem);
    ll_append(mem, (void*) 4);
    ll_append(mem, o_mem);
    o_mem[0] = alt_opcode; o_mem[1] = r2; ((uint16_t*)o_mem)[1] = imm1;
  } else if (r_type == T_REGISTER) {
    uint8_t r1 = _cl_parse_register(r1_raw);
    if (r1 & 0x20 && r2 & 0x20) return "Both operands cannot be pointers";
    if ((r1 & 0x40) ^ (r2 & 0x40)) return "Operands sizes must match";
    uint8_t *o_mem = emalloc(3);
    l_list *mem = ll_make();
    ADDR_ADD(3);
    ll_append(section, mem);
    ll_append(mem, (void*) 3);
    ll_append(mem, o_mem);
    o_mem[0] = opcode; o_mem[1] = r2 | ((r1 & 0x20) >> 1); o_mem[2] = r1;
  } else return "Operand not valid";
  free(r1_raw);
  free(r2_raw);
  return 0;
}
static const char *_cl_parsei_1reg_imm(l_list *line, l_list *section, int opcode, int alt_opcode) {
  if (line->size != 1) return "Needs 1 operand"; //TODO descriptive error 
  char *r1_raw = ll_shift(line);
  uint8_t r_type = _cl_parse_type(r1_raw);
  if (r_type == T_LITERAL) {
    uint32_t r1 = _cl_parse_immediate(r1_raw, section, 1, 2);
    uint8_t *o_mem = emalloc(3);
    l_list *mem = ll_make();
    ADDR_ADD(3);
    ll_append(section, mem);
    ll_append(mem, (void*) 3);
    ll_append(mem, o_mem);
    o_mem[0] = alt_opcode; *((uint16_t*)o_mem + 1) = r1;
  } else if (r_type == T_REGISTER) {
    uint8_t r1 = _cl_parse_register(r1_raw);
    uint8_t *o_mem = emalloc(2);
    l_list *mem = ll_make();
    ADDR_ADD(2);
    ll_append(section, mem);
    ll_append(mem, (void*) 2);
    ll_append(mem, o_mem);
    o_mem[0] = opcode; o_mem[1] = r1;
  } else return "Operand not valid";
  free(r1_raw);
  return 0;
}
static const char *_cl_parsei_1reg_1imm(l_list *line, l_list *section, int opcode) {
  if (line->size != 2) return "Needs 2 operands"; //TODO descriptive error 
  char *r1_raw = ll_shift(line), *r2_raw = ll_shift(line);
  if (_cl_parse_type(r1_raw) != T_LITERAL) return "Operands not valid";
  uint8_t r2 = _cl_parse_register(r2_raw);
  if (r2 == 0xFF) return "Operands not valid";
  uint32_t imm1 = _cl_parse_immediate(r1_raw, section, 2, 1);
  uint8_t *o_mem = emalloc(3);
  l_list *mem = ll_make();
  ADDR_ADD(4);
  ll_append(section, mem);
  ll_append(mem, (void*) 3);
  ll_append(mem, o_mem);
  o_mem[0] = opcode; o_mem[1] = r2; o_mem[2] = imm1;
  free(r1_raw);
  free(r2_raw);
  return 0;
}
static const char *_cl_parsei_jcc(l_list *line, l_list *section, int opcode, int condition) {
  if (line->size != 1) return "Needs a label"; //TODO descriptive error 
  char *r1_raw = ll_shift(line);
  if (_cl_parse_type(r1_raw) != T_LITERAL) return "Operand not valid";
  uint32_t r1 = _cl_parse_immediate(r1_raw, section, 2, 2);
  uint8_t *o_mem = emalloc(4);
  l_list *mem = ll_make();
  ADDR_ADD(4);
  ll_append(section, mem);
  ll_append(mem, (void*) 4);
  ll_append(mem, o_mem);
  o_mem[0] = opcode; o_mem[1] = condition; ((uint16_t*)o_mem)[1] = r1;
  free(r1_raw);
  return 0;
}
static const char *_cl_parsei_movcc(l_list *line, l_list *section, int opcode, int condition) {
  if (line->size != 2) return "Needs 2 operands"; //TODO descriptive error 
  char *r1_raw = ll_shift(line), *r2_raw = ll_shift(line);
  uint8_t r1 = _cl_parse_register(r1_raw), r2 = _cl_parse_register(r2_raw);
  if (r1 == 0xFF || r2 == 0xFF) return "Operands not valid";
  if (r1 & 0x20 && r2 & 0x20) return "Both operands cannot be pointers";
  if ((r1 & 0x40) ^ (r2 & 0x40)) return "Operands sizes must match";
  uint8_t *o_mem = emalloc(4);
  l_list *mem = ll_make();
  ADDR_ADD(4);
  ll_append(section, mem);
  ll_append(mem, (void*) 4);
  ll_append(mem, o_mem);
  o_mem[0] = opcode; o_mem[1] = condition; o_mem[2] = r2 | ((r1 & 0x20) >> 1); o_mem[3] = r1;
  free(r1_raw);
  free(r2_raw);
  return 0;
}
static const char *_cl_parsei_pure(l_list *line, l_list *section, int opcode) {
  if (line->size != 0) return "Needs no operands"; //TODO descriptive error 
  uint8_t *o_mem = emalloc(1);
  l_list *mem = ll_make();
  ADDR_ADD(1);
  ll_append(section, mem);
  ll_append(mem, (void*) 1);
  ll_append(mem, o_mem);
  o_mem[0] = opcode;
  return 0;
}

//TODO not related but important: fix hashmap erasing/adding/listing
//TODO .comm .lcomm .globl LT

//Signature convention:
// literal 	= ["*"] , ("word" | "f_line")
// array 	= name , "[" , signature , {", " , signature} , "]"
// set 		= "%" , name , "[" , literal , "]"
// map 		= "#", ["*"] , array
// signature 	= (array | set | map | literal) , ["..."]
// "..." means "0 or more"
// "*" means "needs to be freed". In case of hashmap, key needs to be freed

//Signature args[file, environment] ==> result[*f_line...]
l_list *preprocess(const char *orig, cmpl_env *env) {
 
  //Signatures:
  hashmap *macros = hm_make(); 			// #*macros[operands[*f_line...], *f_line...]]
  hashset *symbols = hs_make(), 		// %symbols[word]
	  *orphan_symbols = hs_make(); 		// %orphan_symbols[*word]
  l_list *files = ll_make(), 			// files[file[*f_line...]...]
	 *result = ll_make(), 			// result[*f_line...]
	 *dl_result = ll_make(), 		// dl_result[*word..., *f_line...]
	 *history = ll_make(); 			// history[word...]
  
  ll_append(files, _pp_format(env->get_file(orig), orig));
skip:
  while(files->data) {
    l_list *file = files->data->value; 	// file[*f_line...]
    while (file->data) {
      l_list *line = ll_shift(file), *r_line = ll_make();
      char *f_name = ll_shift(line);
      size_t f_line = (size_t) ll_shift(line);
      uint8_t full_skip = history->data && !strcmp(history->data->value, "i0"), out_skip = history->size != 0;
      ll_append(r_line, f_name);
      ll_append(r_line, (void*)f_line);

      {
	char *label = ll_shift(line);
	ll_append(r_line, label);
	if (label && !hs_contains(symbols, label))
	    hs_put(symbols, label);
      }

      if (line->data) {
	char *opcode = ll_shift(line);
	ll_append(r_line, opcode);
	if (_s_with(opcode, '.')) {
	  full_skip = history->size == 0 || !strcmp(history->data->value, "i0");
	  if (!strcmp(opcode, ".set") || !strcmp(opcode, ".equ") || !strcmp(opcode, ".equiv")) {
	    const char *result = _pp_parse_set(line, dl_result, history, symbols);
	    if (result > (const char*) 1) ERR(result);
	    full_skip = (size_t) result;

	  } else if (!strcmp(opcode, ".err")) {
	    const char *result = _pp_parse_err(line, dl_result, history);
	    if (result) ERR(result);

	  } else if (!strcmp(opcode, ".include")) {
	    const char *result = _pp_parse_include(line, dl_result, history, env, files);
	    if (result > (const char*) 1) ERR(result);

	    if (result) {
	      free(ll_shift(r_line));
	      ll_shift(r_line);
	      OLABEL_CLEAN;
	      ll_free_val(r_line);
	      ll_free_val(line);
	      goto skip;
	    }

	  } else if (!strcmp(opcode, ".macro")) {
	    if (history->data && ((char*)history->data->value)[0] != 'm') goto dcwa;
	    const char *result = _pp_parse_macro(line, dl_result, history);
	    ll_prepend(history, "m");
	    if (result) ERR(result);

	  } else if (!strcmp(opcode, ".endm")) {
	    if (history->data && ((char*)history->data->value)[0] != 'm') goto dcwa;
	    const char *result = _pp_parse_endm(line, &dl_result, history, macros);
	    if (result > (const char*) 1) ERR(result);
	    full_skip = history->size == 0 || !strcmp(history->data->value, "i0");

	  } else if (!strcmp(opcode, ".irp")) {
	    if (history->data && ((char*)history->data->value)[0] != 'r') goto dcwa;
	    const char *result = _pp_parse_irp(line, dl_result, history);
	    ll_prepend(history, "rn");
	    if (result) ERR(result);

	  } else if (!strcmp(opcode, ".irpc")) {
	    if (history->data && ((char*)history->data->value)[0] != 'r') goto dcwa;
	    const char *result = _pp_parse_irpc(line, dl_result, history);
	    ll_prepend(history, "rc");
	    if (result) ERR(result);

	  } else if (!strcmp(opcode, ".endr")) {
	    if (history->data && ((char*)history->data->value)[0] != 'r') goto dcwa;
	    const char *result = _pp_parse_endr(line, &dl_result, history, files);
	    if (result > (const char*) 1) ERR(result);
	    full_skip = history->size == 0 || !strcmp(history->data->value, "i0");

	    if (result) {
	      free(ll_shift(r_line));
	      ll_shift(r_line);
	      OLABEL_CLEAN;
	      ll_free_val(r_line);
	      ll_free_val(line);
	      goto skip;
	    }

	  } else if (!strcmp(opcode, ".ifdef")) {
	    if (history->data && ((char*)history->data->value)[0] != 'i') goto dcwa;
	    const char *result = _pp_parse_ifdef(line, dl_result, history, symbols);
	    if (result) ERR(result);

	  } else if (!strcmp(opcode, ".ifndef")) {
	    if (history->data && ((char*)history->data->value)[0] != 'i') goto dcwa;
	    const char *result = _pp_parse_ifndef(line, dl_result, history, symbols);
	    if (result) ERR(result);

	  } else if (!strcmp(opcode, ".else")) {
	    if (history->data && ((char*)history->data->value)[0] != 'i') goto dcwa;
	    const char *result = _pp_parse_else(line, dl_result, history);
	    if (result > (const char*) 1) ERR(result);
	    full_skip = history->size == 1 || !strcmp(history->data->value, "i0");

	  } else if (!strcmp(opcode, ".endif")) {
	    if (history->data && ((char*)history->data->value)[0] != 'i') goto dcwa;
	    const char *result = _pp_parse_endif(line, &dl_result, history, files);
	    if (result > (const char*) 1) ERR(result);
	    full_skip = history->size == 0 || !strcmp(history->data->value, "i0");

	    if (result) {
	      free(ll_shift(r_line));
	      ll_shift(r_line);
	      OLABEL_CLEAN;
	      ll_free_val(r_line);
	      ll_free_val(line);
	      goto skip;
	    }

	  } else {
	    full_skip = history->data && !strcmp(history->data->value, "i0");
	  }
	} else if (hm_contains(macros, opcode)) {
	  l_list *o_sl_data = hm_get(macros, opcode)->value;
	  const char *result = _pp_macro_sub(line, o_sl_data, files);
	  if (result) ERR(result);

	  free(ll_shift(r_line));
	  ll_shift(r_line);
	  OLABEL_CLEAN;
	  ll_free_val(r_line);
	  ll_free_val(line);
	  goto skip;
	}
      }
dcwa:
      while(line->data) {
	ll_append(r_line, ll_shift(line));
      }
      ll_free(line);

      if (!full_skip) {
	if (!out_skip) ll_append(result, r_line);
	else ll_append(dl_result, r_line);
      } else {
	free(ll_shift(r_line));
	ll_shift(r_line);
	OLABEL_CLEAN;
	if (r_line->data && (!strcmp(r_line->data->value, ".set") || 
			     !strcmp(r_line->data->value, ".equ"))) {
	  if (!hs_contains(orphan_symbols, r_line->data->next->value)) {
	    hs_put(orphan_symbols, r_line->data->next->value);
	    r_line->data->next->value = 0;
	  }
	}
	ll_free_val(r_line);
      }
    }
    ll_shift(files);
    ll_free(file);
  }
  
  {
    l_list *s_macros = hm_free_to(macros);
    while (s_macros->data) {
      l_list *macro = ll_shift(s_macros);
      free(ll_shift(macro));
      l_list *data = ll_shift(macro);
      ll_free_val(ll_shift(data));
      ll_free_val(data);
      ll_free_val(macro);
    }
    hs_free(symbols);
    hs_free_val(orphan_symbols);
    ll_free(files);
    ll_free_val(dl_result);
    ll_free(history);
  }
  return result;
}

//Signature (data[*f_line...])==>data[#*symbol[address, section], sections[unwrap$[*name, section[size, symbol[address, *name, size]..., *bytecode]]...]]
l_list *compile(l_list *data, cmpl_env *env) {
  //Signature result[#*symbol[address, section], #*section[address, 32-bit, symbol[address, *name, size]..., *raw_code...]]
  l_list *result = ll_make(), *section;
  char *s_name;
  {
    hashmap *sections = hm_make();
    ll_append(result, hm_make());
    ll_append(result, sections);
    s_name = emalloc(6);
    strcpy(s_name, ".text");
    section = ll_make();
    hm_put(sections, s_name, section);
    ll_append(section, 0);
    ll_append(section, (void*)1);
    ll_append(section, ll_make());
  }

  while(data->data) {
    l_list *line = ll_shift(data);
    char *f_name = ll_shift(line);
    size_t f_line = (size_t) ll_shift(line);
    {
      char *label = ll_shift(line);
      if (label) {
	if (!hm_contains(result->data->value, label)) {
	  l_list *r_label = ll_make();
	  ll_append(r_label, section->data->value);
	  ll_append(r_label, s_name);
	  hm_put(result->data->value, label, r_label);
	} else {
	  ERR("Label redefinition");
	}
      }
    }
    
    if (line->data) {
      char *opcode = ll_shift(line);
      if (_s_with(opcode, '.')) {
	if (!strcmp(opcode, ".byte")) {
	  _cl_parse_int(line, section, 1);
	} else if (!strcmp(opcode, ".short")) {
	  _cl_parse_int(line, section, 2);
	} else if (!strcmp(opcode, ".int")) {
	  _cl_parse_int(line, section, 4);
	} else if (!strcmp(opcode, ".long")) {
	  _cl_parse_int(line, section, 8);
	} else if (!strcmp(opcode, ".single") || !strcmp(opcode, ".float")) {
	  _cl_parse_float(line, section);
	} else if (!strcmp(opcode, ".double")) {
	  _cl_parse_double(line, section);
	} else if (!strcmp(opcode, ".ascii")) {
	  _cl_parse_ascii(line, section);
	} else if (!strcmp(opcode, ".asciz")) {
	  _cl_parse_asciz(line, section);
	} else if (!strcmp(opcode, ".space") || !strcmp(opcode, ".skip")) {
	  const char *err = _cl_parse_skip(line, section);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, ".fill")) {
	  const char *err = _cl_parse_fill(line, section);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, ".align")) {
	  const char *err = _cl_parse_align(line, section);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, ".org")) {
	  const char *err = _cl_parse_org(line, section);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, ".code16")) {
	  if (line->data) ERR(".code16 requires no arguments");
	  section->data->next->value = 0;
	} else if (!strcmp(opcode, ".code32")) {
	  if (line->data) ERR(".code32 requires no arguments");
	  section->data->next->value = (void*) 1;
	} else if (!strcmp(opcode, ".set") || !strcmp(opcode, ".equ")) {
	  const char *err = _cl_parse_equ(line, section, result->data->value);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, ".equiv")) {
	  const char *err = _cl_parse_equiv(line, section, result->data->value);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, ".section")) {
	  if (line->size != 1) ERR(".section requires 1 operand");
	  char *name = ll_shift(line);
	  _cl_parse_section(result->data->next->value, name, &section, &s_name);
	  free(name);
	} else if (!strcmp(opcode, ".data")) {
	  if (line->size != 0) ERR(".data requires no operands");
	  _cl_parse_section(result->data->next->value, ".data", &section, &s_name);
	} else if (!strcmp(opcode, ".text")) {
	  _cl_parse_section(result->data->next->value, ".text", &section, &s_name);
	} else if (!strcmp(opcode, ".comm")) {

	} else if (!strcmp(opcode, ".lcomm")) {

	} else if (!strcmp(opcode, ".globl")) {

	} else {
	  ERR("Unknown directive");
	}
      } else {
	if (!strcmp(opcode, "nop")) {
	  const char *err = _cl_parsei_pure(line, section, I_NOP);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "sleep")) {
	  const char *err = _cl_parsei_1reg_imm(line, section, I_SLEEP, I_SLEEPI);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "gipc")) {
	  const char *err = _cl_parsei_1reg(line, section, I_GIPC);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "mov")) {
	  const char *err = _cl_parsei_2reg_imm(line, section, I_MOV, I_MOVI);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "swap")) {
	  const char *err = _cl_parsei_2reg(line, section, I_SWAP);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "push")) {
	  const char *err = _cl_parsei_1reg(line, section, I_PUSH);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "pop")) {
	  const char *err = _cl_parsei_1reg(line, section, I_POP);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "add")) {
	  const char *err = _cl_parsei_2reg_imm(line, section, I_ADD, I_ADI);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "sub")) {
	  const char *err = _cl_parsei_2reg_imm(line, section, I_SUB, I_SBI);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "mul")) {
	  const char *err = _cl_parsei_2reg(line, section, I_MUL);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "div")) {
	  const char *err = _cl_parsei_2reg(line, section, I_DIV);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "mod")) {
	  const char *err = _cl_parsei_2reg(line, section, I_MOD);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "inc")) {
	  const char *err = _cl_parsei_1reg(line, section, I_INC);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "dec")) {
	  const char *err = _cl_parsei_1reg(line, section, I_DEC);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "fadd")) {
	  const char *err = _cl_parsei_2reg(line, section, I_FADD);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "fsub")) {
	  const char *err = _cl_parsei_2reg(line, section, I_FSUB);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "fmul")) {
	  const char *err = _cl_parsei_2reg(line, section, I_FMUL);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "fdiv")) {
	  const char *err = _cl_parsei_2reg(line, section, I_FDIV);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "i2f")) {
	  const char *err = _cl_parsei_2reg(line, section, I_I2F);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "f2i")) {
	  const char *err = _cl_parsei_2reg(line, section, I_F2I);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "xor")) {
	  const char *err = _cl_parsei_2reg_imm(line, section, I_XOR, I_XORI);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "or")) {
	  const char *err = _cl_parsei_2reg_imm(line, section, I_OR, I_ORI);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "and")) {
	  const char *err = _cl_parsei_2reg_imm(line, section, I_AND, I_ANDI);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "not")) {
	  const char *err = _cl_parsei_1reg(line, section, I_NOT);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "bts")) {
	  const char *err = _cl_parsei_1reg_1imm(line, section, I_BTS);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "btr")) {
	  const char *err = _cl_parsei_1reg_1imm(line, section, I_BTR);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "btc")) {
	  const char *err = _cl_parsei_1reg_1imm(line, section, I_BTC);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "shl")) {
	  const char *err = _cl_parsei_1reg_1imm(line, section, I_SHL);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "shr")) {
	  const char *err = _cl_parsei_1reg_1imm(line, section, I_SHR);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "rol")) {
	  const char *err = _cl_parsei_1reg_1imm(line, section, I_ROL);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "ror")) {
	  const char *err = _cl_parsei_1reg_1imm(line, section, I_ROR);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "int")) {
	  const char *err = _cl_parsei_1imm(line, section, I_INT);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "jmp")) {
	  const char *err = _cl_parsei_1reg_imm(line, section, I_JMP, I_JMPI);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "call")) {
	  const char *err = _cl_parsei_1reg_imm(line, section, I_CALL, I_CALLI);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "rjmp")) {
	  const char *err = _cl_parsei_1reg_imm(line, section, I_RJMP, I_RJMPI);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "rcall")) {
	  const char *err = _cl_parsei_1reg_imm(line, section, I_RCALL, I_RCALLI);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "jae")) {
	  const char *err = _cl_parsei_jcc(line, section, I_JNC, 0x80);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "jnae")) {
	  const char *err = _cl_parsei_jcc(line, section, I_JC, 0x80);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "jb")) {
	  const char *err = _cl_parsei_jcc(line, section, I_JC, 0x80);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "jnb")) {
	  const char *err = _cl_parsei_jcc(line, section, I_JNC, 0x80);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "je")) {
	  const char *err = _cl_parsei_jcc(line, section, I_JC, 0x82);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "jne")) {
	  const char *err = _cl_parsei_jcc(line, section, I_JNC, 0x82);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "jge")) {
	  const char *err = _cl_parsei_jcc(line, section, I_JC, 0x31);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "jnge")) {
	  const char *err = _cl_parsei_jcc(line, section, I_JNC, 0x31);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "jl")) {
	  const char *err = _cl_parsei_jcc(line, section, I_JNC, 0x31);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "jnl")) {
	  const char *err = _cl_parsei_jcc(line, section, I_JC, 0x31);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "jc")) {
	  const char *err = _cl_parsei_jcc(line, section, I_JC, 0x80);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "jnc")) {
	  const char *err = _cl_parsei_jcc(line, section, I_JNC, 0x80);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "jo")) {
	  const char *err = _cl_parsei_jcc(line, section, I_JC, 0x81);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "jno")) {
	  const char *err = _cl_parsei_jcc(line, section, I_JNC, 0x81);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "js")) {
	  const char *err = _cl_parsei_jcc(line, section, I_JC, 0x83);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "jns")) {
	  const char *err = _cl_parsei_jcc(line, section, I_JNC, 0x83);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "jz")) {
	  const char *err = _cl_parsei_jcc(line, section, I_JC, 0x82);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "jnz")) {
	  const char *err = _cl_parsei_jcc(line, section, I_JNC, 0x82);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "movae")) {
	  const char *err = _cl_parsei_movcc(line, section, I_MOVNC, 0x80);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "movnae")) {
	  const char *err = _cl_parsei_movcc(line, section, I_MOVC, 0x80);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "movb")) {
	  const char *err = _cl_parsei_movcc(line, section, I_MOVC, 0x80);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "movnb")) {
	  const char *err = _cl_parsei_movcc(line, section, I_MOVNC, 0x80);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "move")) {
	  const char *err = _cl_parsei_movcc(line, section, I_MOVC, 0x82);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "movne")) {
	  const char *err = _cl_parsei_movcc(line, section, I_MOVNC, 0x82);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "movge")) {
	  const char *err = _cl_parsei_movcc(line, section, I_MOVC, 0x31);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "movnge")) {
	  const char *err = _cl_parsei_movcc(line, section, I_MOVNC, 0x31);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "movl")) {
	  const char *err = _cl_parsei_movcc(line, section, I_MOVNC, 0x31);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "movnl")) {
	  const char *err = _cl_parsei_movcc(line, section, I_MOVC, 0x31);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "movc")) {
	  const char *err = _cl_parsei_movcc(line, section, I_MOVC, 0x80);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "movnc")) {
	  const char *err = _cl_parsei_movcc(line, section, I_MOVNC, 0x80);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "movo")) {
	  const char *err = _cl_parsei_movcc(line, section, I_MOVC, 0x81);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "movno")) {
	  const char *err = _cl_parsei_movcc(line, section, I_MOVNC, 0x81);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "movs")) {
	  const char *err = _cl_parsei_movcc(line, section, I_MOVC, 0x83);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "movns")) {
	  const char *err = _cl_parsei_movcc(line, section, I_MOVNC, 0x83);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "movz")) {
	  const char *err = _cl_parsei_movcc(line, section, I_MOVC, 0x82);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "movnz")) {
	  const char *err = _cl_parsei_movcc(line, section, I_MOVNC, 0x82);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "ret")) {
	  const char *err = _cl_parsei_pure(line, section, I_RET);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "in")) {
	  const char *err = _cl_parsei_2reg_imm(line, section, I_IN, I_INI);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "out")) {
	  const char *err = _cl_parsei_2reg_imm(line, section, I_OUT, I_OUTI);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "crld")) {
	  const char *err = _cl_parsei_2reg(line, section, I_CRLD);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "crst")) {
	  const char *err = _cl_parsei_2reg(line, section, I_CRST);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "sei")) {
	  const char *err = _cl_parsei_pure(line, section, I_SEI);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "cli")) {
	  const char *err = _cl_parsei_pure(line, section, I_CLI);
	  if (err) ERR(err);
	} else if (!strcmp(opcode, "iret")) {
	  const char *err = _cl_parsei_pure(line, section, I_IRET);
	  if (err) ERR(err);
	} else {
	  ERR("Invalid opcode");
	}     
      }
      free(opcode);
    }

    free(f_name);
    ll_free_val(line);
  }
  ll_free(data);
  {
    hashmap *sections = ll_pop(result);
    l_list *f_sections = hm_free_to(sections);
    
    for (struct _le *entry = f_sections->data; entry; entry = entry->next) {
      l_list *sect = ((l_list*)entry->value)->data->next->value;
      size_t sect_size = (size_t) ll_shift(sect);
      ll_shift(sect);
      void* nn1 = ll_shift(sect);
      uint8_t *o_product = emalloc(sect_size), *product = o_product;
      while(sect->data) {
	l_list *part = ll_shift(sect);
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

    ll_append(result, f_sections);
  }
  return result;
}

char *link(l_list *data, cmpl_env *env) {
  return 0; 
}

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

static void _clear_ws(char **data) {
  uint8_t flag = 1, par = 0;
  size_t size = 1;
  for (char *src = *data; *src; src++) {
    if (_is_space(*src)) {
      if (!flag || par) {flag = 1; size++; }
    } else {
      if (flag && *src == '"') par = 1;
      else if (par && *src == '"' && *(src - 1) != '\\') par = !_is_space(*(src+1));
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
      if (flag && *src == '"') par = 1;
      else if (par && *src == '"' && *(src-1) != '\\') par = !_is_space(*(src+1));
      if (*src != ',' || par) *dest++ = *src;
      if ((*src == ',' && !par) || *src == ':') { flag = 1; *dest++ = ' '; }
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
      if (_s_with(word, '\"')) {
	size_t total_len = strlen(word) - 1;
	char *result = emalloc(total_len + 1);
	strcpy(result, word + 1);
	free(word);

	while (1) {
	  if (!line->data) break;
	  word = ll_shift(line);

	  size_t word_len = strlen(word);
	  if (_e_with(word, '\"')) {
	    char *new_result = emalloc(total_len + word_len + 1);
	    memcpy(new_result, result, total_len);
	    memcpy(new_result + total_len + 1, word, word_len - 1);
	    new_result[total_len + word_len] = '\0';
	    new_result[total_len] = ' ';
	    free(result);
	    free(word);
	    result = new_result;
	    break;
	  } else {
	    char *new_result = emalloc(total_len + word_len + 2);
	    memcpy(new_result, result, total_len);
	    memcpy(new_result + total_len + 1, word, word_len);
	    new_result[total_len + word_len + 1] = '\0';
	    new_result[total_len] = ' ';
	    free(result);
	    free(word);
	    result = new_result;
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

//.macro .irp .irpc .ifdef .include .err PPT
//.byte .short .int .long .single/.float .double .ascii .asciz CT
//.space/.skip .fill .align .org .code16 .code32 CT
//.set/.equ .equiv CT
//.section (.data .text expl) .comm .lcomm .globl LT

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
  hashmap *macros = hm_make(); 			// #*macros[operands[*f_line], *f_line]
  hashset *symbols = hs_make(), 		// %symbols[word]
	  *orphan_symbols = hs_make(); 		// %orphan_symbols[*word]
  l_list *files = ll_make(), 			// files[file[*f_line...]...]
	 *result = ll_make(), 			// result[*f_line...]
	 *dl_result = ll_make(), 		// dl_result[*word..., *f_line...]
	 *history = ll_make(); 			// history[word...]
  
  ll_append(files, _pp_format(env->get_file(orig), orig));
skip:
  while(files->data) {
    l_list *file = ll_shift(files); 	// file[*f_line...]
    while (file->data) {
      l_list *line = ll_shift(file), 	//*f_line
	     *r_line = ll_make(); 	//*f_line
      char *f_name = ll_shift(line);
      size_t f_line = (size_t) ll_shift(line);
      
      uint8_t full_skip = history->data && !strcmp(history->data->value, "i0"), out_skip = history->data != 0;
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
	  if (!strcmp(opcode, ".set") || 
	      !strcmp(opcode, ".equ") ||
	      !strcmp(opcode, ".equiv")) {
	    if (line->size != 2) ERR(".set/.equ requires 2 operands");
	    if (!history->data) {
	      if (!hs_contains(symbols, line->data->value))
		hs_put(symbols, line->data->value);
	    }
	    full_skip = history->data && !strcmp(history->data->value, "i0");
	  } else if (!strcmp(opcode, ".err")) {
	    if (line->size < 1) ERR(".err requires label");
	    if (!history->data) {
	      ERR((char*) ll_shift(line));
	    }
	  } else if (!strcmp(opcode, ".macro")) {
	    if (line->size < 1) ERR(".macro requires label");
	    if (history->data && ((char*)history->data->value)[0] != 'm') goto dcwa;
	    if (!history->data) {
	      ll_append(dl_result, ll_shift(line));
	      l_list *operands = ll_make();
	      while (line->data) ll_append(operands, ll_shift(line));
	      ll_append(dl_result, operands);
	    }
	    ll_prepend(history, "m");
	  } else if (!strcmp(opcode, ".irp")) {
	    if (line->size < 1) ERR(".irp requires label");
	    if (history->data && ((char*)history->data->value)[0] != 'r') goto dcwa;
	    if (!history->data) {
	      ll_append(dl_result, ll_shift(line));
	      l_list *operands = ll_make();
	      while (line->data) ll_prepend(operands, ll_shift(line));
	      ll_append(dl_result, operands);
	    }
	    ll_prepend(history, "rn");
	  } else if (!strcmp(opcode, ".irpc")) {
	    if (line->size != 2) ERR(".irpc requires 2 operands");
	    if (history->data && ((char*)history->data->value)[0] != 'r') goto dcwa;
	    if (!history->data) {
	      ll_append(dl_result, ll_shift(line));
	      ll_append(dl_result, ll_shift(line));
	    }
	    ll_prepend(history, "rc");
	  } else if (!strcmp(opcode, ".ifdef")) {
	    if (line->size != 1) ERR(".ifdef requires 1 operand");
	    if (history->data && ((char*)history->data->value)[0] != 'i') goto dcwa;
	    if (!history->data) {
	      if (hs_contains(symbols, line->data->value)) ll_prepend(history, "i1");  
	      else ll_prepend(history, "i0");
	    } else {
	      if (!strcmp(history->data->value, "i0")) ll_prepend(history, "i0");
	      else if (!strcmp(history->data->value, "i1")) ll_prepend(history, "i1");
	    }
	  } else if (!strcmp(opcode, ".ifndef")) {
	    if (line->size != 1) ERR(".ifndef requires 1 operand");
	    if (history->data && ((char*)history->data->value)[0] != 'i') goto dcwa;
	    if (!history->data) {
	      if (hs_contains(symbols, line->data->value)) ll_prepend(history, "i0");  
	      else ll_prepend(history, "i1");
	    } else {
	      if (!strcmp(history->data->value, "i0")) ll_prepend(history, "i0");
	      else if (!strcmp(history->data->value, "i1")) ll_prepend(history, "i1");
	    }
	  } else if (!strcmp(opcode, ".else")) {
	    if (line->size != 0) ERR(".else requires no operands");
	    if (!history->data) ERR(".else requires .ifdef or .ifndef")
	    if (((char*)history->data->value)[0] != 'i') goto dcwa;
	    if (history->size == 1) {
	      if (!strcmp(history->data->value, "i0")) history->data->value = "i1";
	      else if (!strcmp(history->data->value, "i1")) history->data->value = "i0";
	    }
	    full_skip = history->size == 1;
	  } else if (!strcmp(opcode, ".endif")) {
	    if (line->size != 0) ERR(".endif requires no operands");
	    if (!history->data) ERR(".endif requires .ifdef or .ifndef")
	    if (((char*)history->data->value)[0] != 'i') goto dcwa;
	    ll_shift(history);
	    if (!history->data) {
	      SIDELOAD_O;
	      SIDELOAD(dl_result);
	      dl_result = ll_make();
	      
	      free(ll_shift(r_line));
	      ll_shift(r_line);
	      OLABEL_CLEAN;
	      ll_free_val(r_line);
	      ll_free_val(line);
	      goto skip;
	    }
	  } else if (!strcmp(opcode, ".endr")) {
	    if (line->size != 0) ERR(".endr requires no operands");
	    if (!history->data) ERR(".endr requires .irp or .irpc")
	    if (((char*)history->data->value)[0] != 'r') goto dcwa;
	    const char *type = ll_shift(history);
	    if (!history->data) {
	      const char *o_label = ll_shift(dl_result);
	      l_list *sl_data;
	      SIDELOAD_O;
	      if (!strcmp(type, "rc")) {
		const char *o_operands = ll_shift(dl_result);
		for (int i = strlen(o_operands) - 1; i >= 0; i--) {
		  uint16_t ch = o_operands[i];
		  sl_data = _ll_sub(dl_result, o_label, (char*) &ch);
		  SIDELOAD(sl_data);
		}
		free((void*)o_operands);
	      } else {
		l_list *o_operands = ll_shift(dl_result);
		for (struct _le *operand = o_operands->data; operand; operand = operand->next) {
		  sl_data = _ll_sub(dl_result, o_label, operand->value);
		  SIDELOAD(sl_data);
		}
		ll_free_val(o_operands);
	      }
	      free((void*)o_label);
	      dl_result = ll_make();
	      
	      free(ll_shift(r_line));
	      ll_shift(r_line);
	      OLABEL_CLEAN;
	      ll_free_val(r_line);
	      ll_free_val(line);
	      goto skip;
	    }
	  } else if (!strcmp(opcode, ".endm")) {
	    if (line->size != 0) ERR(".endm requires no operands");
	    if (!history->data) ERR(".endm requires .macro")
	    if (((char*)history->data->value)[0] != 'm') goto dcwa;
	    ll_shift(history);
	    if (!history->data) {
	      const char *o_label = ll_shift(dl_result);
	      hm_put(macros, o_label, dl_result);
	      dl_result = ll_make();
	      full_skip = 1;
	    }
	  } else if (!strcmp(opcode, ".include")) {
	    if (line->size != 1) ERR(".include requires 1 operand");
	    if (!history->data) {
	      l_list *sl_data = _pp_format(env->get_file(line->data->value), line->data->value);
	      SIDELOAD_O;
	      SIDELOAD(sl_data);

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
	  l_list *o_sl_data = hm_get(macros, opcode), *sl_data = ll_make();
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
	    if (se->size > 2) ERR(".macro operand must be 'a=c' or 'a'")
	    else if (c_entry) { with = c_entry->value; c_entry = c_entry->next; }
	    else if (se->size == 2) with = se->data->next->value;
	    else ERR("Macro call requires more arguments");
	    l_list *t_sl_data = _ll_sub(sl_data, what, with);
	    ll_free_val(sl_data);
	    sl_data = t_sl_data;
	    ll_free_val(se);
	  }
	  SIDELOAD_O;
	  SIDELOAD(sl_data);
	  ll_free_val(operands);
	  
	  //TODO currently label on macros is ignored, maybe put it to a new line
	  free(ll_shift(r_line));
	  ll_shift(r_line);
	  OLABEL_CLEAN;
	  ll_free_val(r_line);
	  ll_free_val(line);
	  goto skip;
	}
      }
dcwa: //Dont care + Who asked
      {
	while(line->data) {
	  ll_append(r_line, ll_shift(line));
	}
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
    ll_free(file);
  }
  //FIXME memory leak in "macros": "operands[*f_line]" not freed correctly 
  hm_free_val(macros);
  hs_free(symbols);
  hs_free_val(orphan_symbols);
  ll_free(files);
  ll_free_val(dl_result);
  ll_free(history);
  return result;
}

//Signature (data[*f_line...])==>data[#*symbol[address, section], #section[size, symbol[address, *name, 32-bit]..., *bytecode]]
l_list *compile(l_list *data, cmpl_env *env) {
  //Signature result[#*symbol[address, section], #section[address, 32-bit, symbol[address, *name, 32-bit]..., *raw_code...]]
  l_list *result = ll_make(), *section;
  hashmap *equs = hm_make(); // #*equs[*with]
  //FIXME potential memory leak, can be either static or dynamic
  char *s_name = ".text";
  {
    ll_append(result, hm_make());
    ll_append(result, hm_make());
    section = ll_make();
    hm_put(result->data->next->value, s_name, section);
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
	  ll_append(r_label, section->data->next->value);
	  hm_put(result->data->value, label, r_label);
	} else {
	  ERR("Label redefinition");
	}
      }
    }
    
    if (line->data) {
      char *opcode = ll_shift(data);
      if (_s_with(opcode, '.')) {
	if (!strcmp(opcode, ".byte")) {
	  if (line->size > 0) {
	    int8_t *o_mem = emalloc(line->size);
	    l_list *mem = ll_make();
	    ADDR_ADD(line->size);
	    ll_append(section, mem);
	    ll_append(mem, (void*) line->size);
	    ll_append(mem, o_mem);
	    while (line->data) {
	      char *operand = ll_shift(line);
	      int i_operand = strtol(operand, 0, 0);
	      *o_mem++ = (int8_t) i_operand;
	      free(operand);
	    }
	  }
	} else if (!strcmp(opcode, ".short")) {
	  if (line->size > 0) {
	    int16_t *o_mem = emalloc(line->size*2);
	    l_list *mem = ll_make();
	    ADDR_ADD(line->size*2);
	    ll_append(section, mem);
	    ll_append(mem, (void*) (line->size*2));
	    ll_append(mem, o_mem);
	    while (line->data) {
	      char *operand = ll_shift(line);
	      int i_operand = strtol(operand, 0, 0);
	      *o_mem++ = (int16_t) i_operand;
	      free(operand);
	    }
	  }
	} else if (!strcmp(opcode, ".int")) {
	  if (line->size > 0) {
	    int32_t *o_mem = emalloc(line->size*4);
	    l_list *mem = ll_make();
	    ADDR_ADD(line->size*4);
	    ll_append(section, mem);
	    ll_append(mem, (void*) (line->size*4));
	    ll_append(mem, o_mem);
	    while (line->data) {
	      char *operand = ll_shift(line);
	      int i_operand = strtol(operand, 0, 0);
	      *o_mem++ = (int32_t) i_operand;
	      free(operand);
	    }
	  }
	} else if (!strcmp(opcode, ".long")) {
	  if (line->size > 0) {
	    int64_t *o_mem = emalloc(line->size*8);
	    l_list *mem = ll_make();
	    ADDR_ADD(line->size*8);
	    ll_append(section, mem);
	    ll_append(mem, (void*) (line->size*8));
	    ll_append(mem, o_mem);
	    while (line->data) {
	      char *operand = ll_shift(line);
	      long long int i_operand = strtoll(operand, 0, 0);
	      *o_mem++ = (int64_t) i_operand;
	      free(operand);
	    }
	  }
	} else if (!strcmp(opcode, ".single") ||
	    !strcmp(opcode, ".float")) {
	  if (line->size > 0) {
	    float *o_mem = emalloc(line->size*4);
	    l_list *mem = ll_make();
	    ADDR_ADD(line->size*4);
	    ll_append(section, mem);
	    ll_append(mem, (void*) (line->size*4));
	    ll_append(mem, o_mem);
	    while (line->data) {
	      char *operand = ll_shift(line);
	      float i_operand = strtof(operand, 0);
	      *o_mem++ = (float) i_operand;
	      free(operand);
	    }
	  }
	} else if (!strcmp(opcode, ".double")) {
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
	      *o_mem++ = (double) i_operand;
	      free(operand);
	    }
	  }
	} else if (!strcmp(opcode, ".ascii")) {
	  if (line->size > 0) {
	    size_t calc_size = 0;
	    for (struct _le *entry = line->data; entry; entry = entry->next)
	      calc_size += strlen(entry->value);
	    char *o_mem = emalloc(calc_size);
	    l_list *mem = ll_make();
	    ADDR_ADD(calc_size);
	    ll_append(section, mem);
	    ll_append(mem, (void*) calc_size);
	    ll_append(mem, o_mem);
	    while (line->data) {
	      char *operand = ll_shift(line);
	      size_t loc_size = strlen(operand);
	      memcpy(o_mem, operand, loc_size);
	      o_mem += loc_size;
	      free(operand);
	    }
	  }
	} else if (!strcmp(opcode, ".asciz")) {
	  if (line->size > 0) {
	    size_t calc_size = 0;
	    for (struct _le *entry = line->data; entry; entry = entry->next)
	      calc_size += strlen(entry->value) + 1;
	    char *o_mem = emalloc(calc_size);
	    l_list *mem = ll_make();
	    ADDR_ADD(calc_size);
	    ll_append(section, mem);
	    ll_append(mem, (void*) calc_size);
	    ll_append(mem, o_mem);
	    while (line->data) {
	      char *operand = ll_shift(line);
	      size_t loc_size = strlen(operand) + 1;
	      memcpy(o_mem, operand, loc_size);
	      o_mem += loc_size;
	      free(operand);
	    }
	  }
	} else if (!strcmp(opcode, ".space") ||
	    !strcmp(opcode, ".skip")) {
	  if (line->size > 2 || line->size < 1) ERR(".space/.skip requires 2 or 1 operands");
	  char *o_size = ll_shift(line);
	  size_t size = strtol(o_size, 0, 0), val = 0;
	  free(o_size);
	  if (line->data) {
	    char *o_val = ll_shift(line);
	    val = strtol(o_val, 0, 0);
	    free(o_val);
	  }
	  if (!size) ERR(".space/.skip requires a valid size");
	  int8_t *o_mem = emalloc(size);
	  l_list *mem = ll_make();
	  ADDR_ADD(size);
	  ll_append(section, mem);
	  ll_append(mem, (void*) size);
	  ll_append(mem, o_mem);
	  memset(o_mem, (int8_t)val, size);
	} else if (!strcmp(opcode, ".fill")) {
	  if (line->size > 3 || line->size < 1) ERR(".fill requires between 3 and 1 operands");
	  char *o_repeat = ll_shift(line);
	  size_t repeat = strtol(o_repeat, 0, 0), size = 1, val = 0;
	  free(o_repeat);
	  if (line->data) {
	    char *o_size = ll_shift(line);
	    size = strtol(o_size, 0, 0);
	    free(o_size);
	    if(!size) size = 1;
	    else if (size > 8) ERR(".fill requires a size less or equal than 8");
	  }
	  if (line->data) {
	    char *o_val = ll_shift(line);
	    val = strtol(o_val, 0, 0);
	    free(o_val);
	  }
	  if (!repeat) ERR(".fill requires a valid repeat");
	  int8_t *o_mem = emalloc(size*repeat);
	  l_list *mem = ll_make();
	  ADDR_ADD(size*repeat);
	  ll_append(section, mem);
	  ll_append(mem, (void*) (size*repeat));
	  ll_append(mem, o_mem);
	  for (size_t i = 0; i < size*repeat; i += size) {
	    memcpy(o_mem+i, &val, size);
	  }
	} else if (!strcmp(opcode, ".align")) {
	  //TODO .balign and .p2align
	  if (line->size > 3 || line->size < 1) ERR(".align requires between 3 and 1 operands");
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
	  if (!align) ERR(".align requires a valid alignment");
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
	} else if (!strcmp(opcode, ".org")) {
	  if (line->size > 2 || line->size < 1) ERR(".org requires 2 or 1 operands");
	  char *o_until = ll_shift(line);
	  size_t until = strtol(o_until, 0, 0), val = 0;
	  free(o_until);
	  if (line->data) {
	    char *o_val = ll_shift(line);
	    val = strtol(o_val, 0, 0);
	    free(o_val);
	  }
	  if (!until) ERR(".org requires a valid address");
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
	} else if (!strcmp(opcode, ".code16")) {
	  if (line->data) ERR(".code16 requires no arguments");
	  section->data->next->value = 0;
	} else if (!strcmp(opcode, ".code32")) {
	  if (line->data) ERR(".code32 requires no arguments");
	  section->data->next->value = (void*) 1;
	} else if (!strcmp(opcode, ".set") ||
	    !strcmp(opcode, ".equ")) {
	  if (line->size != 2) ERR(".set/.equ requires 2 operands");

	} else if (!strcmp(opcode, ".equiv")) {
	  if (line->size != 2) ERR(".equiv requires 2 operands");

	} else if (!strcmp(opcode, ".section")) {

	} else if (!strcmp(opcode, ".data")) {

	} else if (!strcmp(opcode, ".text")) {

	} else if (!strcmp(opcode, ".comm")) {

	} else if (!strcmp(opcode, ".lcomm")) {

	} else if (!strcmp(opcode, ".globl")) {

	} else {

	}
      } else {

      }
      free(opcode);
    }

    free(f_name);
    ll_free_val(line);
  }
  ll_free(data);
  return result;
}

char *link(l_list *data, cmpl_env *env) {
  return 0; 
}

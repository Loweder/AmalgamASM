#include "aasm/compile.h"
#include <stdio.h>
#include <string.h>

#define SIDELOAD_O \
  ll_prepend(files, data);
#define SIDELOAD(our_data) \
  ll_prepend(files, our_data);

#define ERR(code) { snprintf(env->err, 100, "%s:%ld: %s", f_name, f_line, code); return 0; }
#define OLABEL_CLEAN {\
  char *o_label = ll_shift(r_line);\
  if (o_label && !hs_contains(orphan_symbol_data, o_label))\
    hs_put(orphan_symbol_data, o_label);\
  else free(o_label);\
} 

void _debug_print_line(l_list *line) {
  char *file_name = line->data->value;
  size_t line_number = (size_t) line->data->next->value;

  printf("%s:%ld ", file_name, line_number);

  for(struct _le *entry = line->data->next->next; entry; entry = entry->next) {
    printf("%s ", (char*) entry->value);
  }
  printf("\n");
}

void _debug_print_lines(l_list *lines) {
  for (struct _le *line = lines->data; line; line = line->next) {
    _debug_print_line(line->value);
  }
}

char *_get_token(char **data, char delim) {
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

uint8_t _s_with(const char *data, char sym) {
  return *data == sym;
}

uint8_t _e_with(const char *data, char sym) {
  char *inter = strchr(data, sym);
  return inter && !*(inter + 1);
}

uint8_t _is_space(char datum) {
  return datum == ' ' || datum == '\t' || datum == '\0';
}

void _clear_ws(char **data) {
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

l_list *_ll_from(const char *o_data, char delim) {
  const char *data = o_data;
  l_list *result = ll_make();
  while(*data) {
    ll_append(result, _get_token((char**)&data, delim));
  }
  free((void*)o_data);
  return result;
}

l_list *_copy_pos(l_list *line) {
  l_list *result = ll_make();
  
  char *f_name = emalloc(strlen(line->data->value) + 1);
  strcpy(f_name, line->data->value);
  ll_append(result, f_name);
  ll_append(result, line->data->next->value);

  return result;
}

l_list *_copy_line(l_list *line) {
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

l_list *_ll_sub(l_list *orig, const char *what, const char *with) {
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

l_list *_pp_format(const char *o_data, const char *name) {
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

//.macro .irp .irpc .ifdef .include PPT
//.byte .short .int .long .single/.float .double .ascii .asciz CT
//.space/.skip .fill .align .org .code16 .code32 CT
//.set/.equ .equiv CT
//.section (.data .text expl) .comm .lcomm .globl LT

//Naming convention: 
//f* = files, l* = line, w* = word, o* = original, r* = result
//dl_table	= directive line table (contents of .ifdef .else .macro .irp)
//hs_table 	= history table (stores history of control directives)

l_list *preprocess(const char *orig, cmpl_env *env) {
  hashmap *macro_data = hm_make();
  hashset *symbol_data = hs_make(), *orphan_symbol_data = hs_make();
  l_list *files = ll_make(), *result = ll_make(), 
	 *dl_result = ll_make(), *history = ll_make();
  
  ll_append(files, _pp_format(env->get_file(orig), orig));
skip:
  while(files->data) {
    l_list *data = ll_shift(files);
    while (data->data) {
      l_list *line = ll_shift(data), *r_line = ll_make();
      char *f_name = ll_shift(line);
      size_t f_line = (size_t) ll_shift(line);
      uint8_t full_skip = history->data && !strcmp(history->data->value, "i0"), out_skip = history->data != 0;
      ll_append(r_line, f_name);
      ll_append(r_line, (void*)f_line);

      {
	char *l_label = ll_shift(line);
	ll_append(r_line, l_label);
	if (l_label && !hs_contains(symbol_data, l_label))
	    hs_put(symbol_data, l_label);
      }

      if (line->data) {
	char *l_opcode = ll_shift(line);
	ll_append(r_line, l_opcode);
	if (_s_with(l_opcode, '.')) {
	  full_skip = history->size == 0 || !strcmp(history->data->value, "i0");
	  if (!strcmp(l_opcode, ".set") || 
	      !strcmp(l_opcode, ".equ")) {
	    if (line->size != 2) ERR(".set/.equ requires 2 operands");
	    if (!history->data) {
	      if (!hs_contains(symbol_data, line->data->value))
		hs_put(symbol_data, line->data->value);
	    }
	    full_skip = history->data && !strcmp(history->data->value, "i0");
	  } else if (!strcmp(l_opcode, ".macro")) {
	    if (line->size < 1) ERR(".macro requires label");
	    if (history->data && ((char*)history->data->value)[0] != 'm') goto dcwa;
	    if (!history->data) {
	      ll_append(dl_result, ll_shift(line));
	      l_list *operands = ll_make();
	      while (line->data) ll_append(operands, ll_shift(line));
	      ll_append(dl_result, operands);
	    }
	    ll_prepend(history, "m");
	  } else if (!strcmp(l_opcode, ".irp")) {
	    if (line->size < 1) ERR(".irp requires label");
	    if (history->data && ((char*)history->data->value)[0] != 'r') goto dcwa;
	    if (!history->data) {
	      ll_append(dl_result, ll_shift(line));
	      l_list *operands = ll_make();
	      while (line->data) ll_prepend(operands, ll_shift(line));
	      ll_append(dl_result, operands);
	    }
	    ll_prepend(history, "rn");
	  } else if (!strcmp(l_opcode, ".irpc")) {
	    if (line->size != 2) ERR(".irpc requires 2 operands");
	    if (history->data && ((char*)history->data->value)[0] != 'r') goto dcwa;
	    if (!history->data) {
	      ll_append(dl_result, ll_shift(line));
	      ll_append(dl_result, ll_shift(line));
	    }
	    ll_prepend(history, "rc");
	  } else if (!strcmp(l_opcode, ".ifdef")) {
	    if (line->size != 1) ERR(".ifdef requires 1 operand");
	    if (history->data && ((char*)history->data->value)[0] != 'i') goto dcwa;
	    if (!history->data) {
	      if (hs_contains(symbol_data, line->data->value)) ll_prepend(history, "i1");  
	      else ll_prepend(history, "i0");
	    } else {
	      if (!strcmp(history->data->value, "i0")) ll_prepend(history, "i0");
	      else if (!strcmp(history->data->value, "i1")) ll_prepend(history, "i1");
	    }
	  } else if (!strcmp(l_opcode, ".ifndef")) {
	    if (line->size != 1) ERR(".ifndef requires 1 operand");
	    if (history->data && ((char*)history->data->value)[0] != 'i') goto dcwa;
	    if (!history->data) {
	      if (hs_contains(symbol_data, line->data->value)) ll_prepend(history, "i0");  
	      else ll_prepend(history, "i1");
	    } else {
	      if (!strcmp(history->data->value, "i0")) ll_prepend(history, "i0");
	      else if (!strcmp(history->data->value, "i1")) ll_prepend(history, "i1");
	    }
	  } else if (!strcmp(l_opcode, ".else")) {
	    if (line->size != 0) ERR(".else requires no operands");
	    if (!history->data) ERR(".else requires .ifdef or .ifndef")
	    if (((char*)history->data->value)[0] != 'i') goto dcwa;
	    if (history->size == 1) {
	      if (!strcmp(history->data->value, "i0")) history->data->value = "i1";
	      else if (!strcmp(history->data->value, "i1")) history->data->value = "i0";
	    }
	    full_skip = history->size == 1;
	  } else if (!strcmp(l_opcode, ".endif")) {
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
	  } else if (!strcmp(l_opcode, ".endr")) {
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
	  } else if (!strcmp(l_opcode, ".endm")) {
	    if (line->size != 0) ERR(".endm requires no operands");
	    if (!history->data) ERR(".endm requires .macro")
	    if (((char*)history->data->value)[0] != 'm') goto dcwa;
	    ll_shift(history);
	    if (!history->data) {
	      const char *o_label = ll_shift(dl_result);
	      hm_put(macro_data, o_label, dl_result);
	      dl_result = ll_make();
	      full_skip = 1;
	    }
	  } else if (!strcmp(l_opcode, ".include")) {
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
	} else if (hm_contains(macro_data, l_opcode)) {
	  l_list *o_sl_data = hm_get(macro_data, l_opcode), *sl_data = ll_make();
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
	  if (!hs_contains(orphan_symbol_data, r_line->data->next->value)) {
	    hs_put(orphan_symbol_data, r_line->data->next->value);
	    r_line->data->next->value = 0;
	  }
	}
	ll_free_val(r_line);
      }
    }
    ll_free(data);
  }
  printf("-------------------------------\n");
  ll_free(files);
  ll_free(history);
  ll_free_val(dl_result);
  hm_free_val(macro_data);
  hs_free(symbol_data);
  return result;
}

l_list *compile(l_list *o_data, cmpl_env *env) {
  //l_list *data = to_llist(o_data, '\n'), *l_table = ll_make();
  //while(data->data) {
  //  l_list *o_line = to_llist(ll_shift(data), ' ');
  //  
  //  ll_free_val(o_line);
  //}
  //ll_free_val(data);
  //return from_llist_ir(l_table);
}

char *link(l_list *data, cmpl_env *env) {
  
}

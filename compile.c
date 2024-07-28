#include "aasm/compile.h"
#include "aasm/util.h"
#include <stdio.h>
#include <string.h>

#define SIDELOAD_O \
  ll_prepend(f_table, data);\
  ll_prepend(of_table, o_data);
#define SIDELOAD(our_data) \
  ll_prepend(f_table, our_data);\
  ll_prepend(of_table, our_data);

#define STR_COPY(dest, src) \
  dest = emalloc(strlen(src));\
  strcpy(dest, src);

#define ERR(code) { snprintf(env->err, 50, "%d: %s", f_line, code); return 0; }

char *gettoken(char **data, char delim) {
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

void clear_ws(char **data) {
  uint8_t flag = 1;
  size_t size = 1;
  for (char *src = *data; *src; src++) {
    if (*src == '\t' || *src == ' ') {
      if (!flag) {flag = 1; size++; }
    } else {
      if (*src == ',' || *src == ':') flag = 1;
      else flag = 0;
      size += (*src == ':') ? 2 : 1;
    }
  }
  if (flag && size > 1) size--;
  char *o_dest = NEW_A(char, size), *dest = o_dest;
  flag = 1;

  for(char *src = *data; *src; src++) {
    if (*src == '\t' || *src == ' ') {
      if (!flag) { flag = 1; *dest++ = ' '; }
    } else {
      if (*src == ',' || *src == ':') flag = 1;
      else flag = 0;
      *dest++ = *src;
      if (*src == ':') *dest++ = ' ';
    } 
  }
  if (flag && size > 1) dest--;
  *dest = '\0';
  free(*data);
  *data = o_dest;
}

uint8_t s_with(const char *data, char sym) {
  return *data == sym;
}

uint8_t e_with(const char *data, char sym) {
  char *inter = strchr(data, sym);
  return inter && !*(inter + 1);
}

char *from_llist(l_list *list, char delim) {
  size_t size = 1;
  for (struct _le *l_last = list->data; l_last; l_last = l_last->next) {
    size += strlen(l_last->str) + 1;
  }
  char *o_result = emalloc(size), *result = o_result;
  for (struct _le *l_last = list->data; l_last; l_last = l_last->next) {
    strcpy(result, l_last->str);
    result += strlen(l_last->str);
    *(result++) = delim;
  }
  *(--result) = '\0';
  ll_free(list);
  return o_result;
}

l_list *to_llist(const char *o_data, char delim) {
  const char *data = o_data;
  l_list *result = ll_make();
  while(*data) {
    ll_append(result, gettoken((char**)&data, delim));
  }
  free((void*)o_data);
  return result;
}

char *substitute(const char *o_src, const char *what, const char *with) {
  size_t what_size = strlen(what), with_size = strlen(with);
  int size_diff = (int)(with_size - (what_size+1));
  size_t size = strlen(o_src) + 1;
  const char *src = o_src;
  while(*src) {
    if (*src++ == '\\' && !memcmp(src, what, what_size)) {
      size += size_diff;
      src += what_size;
    }
  }
  char *o_result = emalloc(size), *result = o_result;
  src = o_src;
  
  while(*src) {
    if (*src == '\\' && !memcmp(src + 1, what, what_size)) {
      memcpy(result, with, with_size);
      src += what_size+1;
      result += with_size;
    } else {
      *result = *src;
      src++; result++;
    }
  }
  *result = '\0';
  return o_result;
}

//.macro .irp .irpc .ifdef .include PPT
//.byte .short .int .long .single/.float .double .ascii .asciz CT
//.space/.skip .fill .align .org .code16 .code32 CT
//.set/.equ .equiv CT
//.section (.data .text expl) .comm .lcomm .globl LT

//WARNING: Will free "orig"
//Naming convention: 
//f* = file, l* = line, o* = original
//mac_table 	= macro table
//sym_table 	= symbol table
//dl_table	= directive line table (contents of .ifdef .else .macro .irp)
//hs_table 	= history table (stores history of control directives)
char *preprocess(const char *orig, cmpl_env *env) {
  uint32_t f_line = 0;
  hashmap *mac_table = hm_make();
  hashset *sym_table = hs_make();
  l_list *hs_table = ll_make(), *dl_table = ll_make(),
	 *l_table = ll_make(), *f_table = ll_make(), *of_table = ll_make();
  ll_prepend(f_table, orig);
  ll_prepend(of_table, orig);

skip:
  while (f_table->data) {
    const char *o_data = ll_shift(of_table);
    const char *data = ll_shift(f_table);
    while (*data) {
      f_line++;
      char *label = 0, *opcode = 0, *operands = 0;
      {
	char *line = gettoken((char**)&data, '\n');
	clear_ws(&line);
	if (s_with(line, '#') || s_with(line, '\0')) {
	  free(line);
	  continue;
	}
	l_list *o_words = to_llist(line, ' ');
	struct _le *words = o_words->data;

	if (words && e_with(words->str, ':')) {
	  STR_COPY(label, words->str); words = words->next;
	  label[strlen(label) - 1] = '\0';
	}
	if (words && !s_with(words->str, '#')) {
	  STR_COPY(opcode, words->str); words = words->next;
	  if (words && s_with(opcode, '.') && !label && !s_with(words->str, '#')) {
	    STR_COPY(label, words->str); words = words->next;
	  }
	}
	if (words && !s_with(words->str, '#')) {
	  STR_COPY(operands, words->str); words = words->next;
	}
	if (words && !s_with(words->str, '#')) ERR("Expecting EOL or comment, got symbol");
	ll_free(o_words);	
      }

      uint8_t full_skip = hs_table->data && !strcmp(hs_table->data->str, "i0"), out_skip = hs_table->data != 0;
      size_t l_size = (opcode ? strlen(opcode) : 0) + (label ? strlen(label) : 0) + (operands ? strlen(operands) : 0) + 3;
      char *line = emalloc(l_size);
      snprintf(line, l_size, "%s %s %s", opcode ? opcode : "", label ? label : "", operands ? operands : "");
      if (opcode && s_with(opcode, '.')) {
	full_skip = hs_table->size == 0;
	if (!strcmp(opcode, ".macro")) {
	  if (!label) ERR(".macro requires label");
	  ll_prepend(hs_table, "m");
	  if (operands) {
	    ll_prepend(dl_table, operands);
	    operands = 0;
	  } else ll_prepend(dl_table, "");
	  ll_prepend(dl_table, label);

	} else if (!strcmp(opcode, ".irpc")) {
	  if (!label) ERR(".irpc requires label");
	  if (!operands) ERR(".irpc requires operands");
	  ll_prepend(hs_table, "r");
	  ll_prepend(dl_table, operands);
	  ll_prepend(dl_table, label);
	  operands = 0;

	} else if (!strcmp(opcode, ".irp")) {
	  if (!label) ERR(".irp requires label");
	  if (operands) ERR(".irp requires no operands");
	  ll_prepend(hs_table, "r");
	  ll_prepend(dl_table, "");
	  ll_prepend(dl_table, label);

	} else if (!strcmp(opcode, ".ifdef")) {
	  if (!label) ERR(".ifdef requires label");
	  if (operands) ERR(".ifdef requires no operands");
	  if (hs_contains(sym_table, label)) ll_prepend(hs_table, "i1");  
	  else ll_prepend(hs_table, "i0");
	  free(label);

	} else if (!strcmp(opcode, ".ifndef")) {
	  if (!label) ERR(".ifndef requires label");
	  if (operands) ERR(".ifndef requires no operands");
	  if (hs_contains(sym_table, label)) ll_prepend(hs_table, "i0");  
	  else ll_prepend(hs_table, "i1");
	  free(label);

	} else if (!strcmp(opcode, ".else")) {
	  if (label) ERR(".else requires no label");
	  if (operands) ERR(".else requires no operands");
	  if (!hs_table->data || (hs_table->data->str[0] != 'i')) ERR(".else requires .ifdef or .ifndef")
	  else if (!strcmp(hs_table->data->str, "i0")) hs_table->data->str = "i1";
	  else if (!strcmp(hs_table->data->str, "i1")) hs_table->data->str = "i0";
	  full_skip = hs_table->size == 1;

	} else if (!strcmp(opcode, ".endm")) {
	  if (label) ERR(".endm requires no label");
	  if (operands) ERR(".endm requires no operands");
	  if (!hs_table->data || (hs_table->data->str[0] != 'm')) ERR(".endm requires .macro")
	  else {ll_shift(hs_table);};
	  if (!hs_table->data) {
	    const char *o_label = ll_shift(dl_table);
	    char *sl_data = from_llist(dl_table, '\n');
	    hm_put(mac_table, o_label, sl_data);
	    dl_table = ll_make();
	    full_skip = 1;
	  }

	} else if (!strcmp(opcode, ".endr")) {
	  if (label) ERR(".endr requires no label");
	  if (operands) ERR(".endr requires no operands");
	  if (!hs_table->data || (hs_table->data->str[0] != 'r')) ERR(".endr requires .irp or .irpc")
	  else {ll_shift(hs_table);};
	  if (!hs_table->data) {
	    const char *o_label = ll_shift(dl_table);
	    char *o_sl_data, *sl_data;
	    SIDELOAD_O;
	    if (dl_table->data && (dl_table->data->str[0] != '\0')) {
	      const char *o_operands = ll_shift(dl_table);
	      o_sl_data = from_llist(dl_table, '\n');
	      for (int i = strlen(o_operands) - 1; i >= 0; i--) {
		short ch = o_operands[i];
		sl_data = substitute(o_sl_data, o_label, (char*) &ch);
		SIDELOAD(sl_data);
	      }
	      free((void*)o_label);
	      free((void*)o_operands);
	    } else {
	      ll_shift(dl_table);
	      l_list *o_operands = to_llist(o_label, ',');
	      o_sl_data = from_llist(dl_table, '\n');
	      if (o_operands->size < 2) ERR(".endr requires 2 or more operands");
	      struct _le* prev = 0;
	      struct _le* current = o_operands->data->next;
	      struct _le* next = 0;

	      while (current) {
		next = current->next;
		current->next = prev;
		prev = current;
		current = next;
	      }
	      o_operands->data->next = prev;

	      for (struct _le *operand = o_operands->data->next; operand; operand = operand->next) {
		sl_data = substitute(o_sl_data, o_operands->data->str, operand->str);
		SIDELOAD(sl_data);
	      }
	      ll_free(o_operands);
	    }
	    dl_table = ll_make();
	    free(opcode);
	    free(line);
	    goto skip;
	  }

	} else if (!strcmp(opcode, ".endif")) {
	  if (label) ERR(".endif requires no label");
	  if (operands) ERR(".endif requires no operands");
	  if (!hs_table->data || (hs_table->data->str[0] != 'i')) ERR(".endif requires .ifdef or .ifndef")
	  else {ll_shift(hs_table);};
	  if (!hs_table->data) {
	    char *sl_data = from_llist(dl_table, '\n');
	    SIDELOAD_O;
	    SIDELOAD(sl_data);
	    dl_table = ll_make();
	    free(opcode);
	    free(line);
	    goto skip;
	  }

	} else if (!strcmp(opcode, ".include")) {
	  if (out_skip)
	    goto clean;
	  if (!label) ERR(".include requires file name");
	  if (operands) ERR(".include requires no operands");
	  const char *sl_data = env->get_file(label);
	  SIDELOAD_O;
	  SIDELOAD(sl_data);
	  free(opcode);
	  free(label);
	  free(line);
	  goto skip;

	} else if (label) {
	  if (!hs_contains(sym_table, label)) hs_put(sym_table, label);
	  else free(label);
	  full_skip = hs_table->data && !strcmp(hs_table->data->str, "i0");
	  //TODO not sure how I feel about symbol redefinition. Maybe throw error?
	}
      } else if (opcode && hm_contains(mac_table, opcode)) {
	const char *o_sl_data = hm_get(mac_table, opcode), *sl_data;
	l_list *o_operands = to_llist(gettoken((char**)&o_sl_data, '\n'), ','),
	       *c_operands = operands ? to_llist(operands, ',') : ll_make();
	sl_data = emalloc(strlen(o_sl_data) + 1);
	strcpy((char*)sl_data, o_sl_data);

	struct _le *c_entry = c_operands->data;
	while (o_operands->data) {
	  l_list *se = to_llist(ll_shift(o_operands), '=');
	  const char *what = se->data->str, *with;
	  if (se->size > 2) ERR(".macro operand must be 'a=c' or 'a'")
	  else if (c_entry) { with = c_entry->str; c_entry = c_entry->next; }
	  else if (se->size == 2) with = se->data->next->str;
	  else ERR("Macro call requires more arguments");
	  sl_data = substitute(sl_data, what, with);
	  ll_free(se);
	}
	SIDELOAD_O;
	SIDELOAD(sl_data);
	ll_free(o_operands);
	ll_free(c_operands);
	free(opcode);
	//TODO currently label on macros is ignored, maybe make it a new line
	free(label);
	free(line);
	goto skip;
      } else if (label) {
	if (!hs_contains(sym_table, label)) hs_put(sym_table, label);
	else free(label);
      }
clean:
      free(operands);
      free(opcode);
      if (!full_skip) {
	if (!out_skip) ll_append(l_table, line);
	else ll_append(dl_table, line);
      }
      else free(line);
    }
    free((void*)o_data);
  }

  ll_free(of_table);
  ll_free(f_table);
  ll_free(dl_table);
  ll_clear(hs_table);
  ll_free(hs_table);
  hs_free(sym_table);
  return from_llist(l_table, '\n');
}

char *compile(const char *data, cmpl_env *env) {
  
}

char *link(const char **data, const char *linker, cmpl_env *env) {
  
}

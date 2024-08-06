#include "aasm/internal/compile.h"



//.comm .lcomm .globl LT

//Signature convention:
// literal 	= ["*"] , ("word" | "f_line")
// array 	= name , "[" , signature , {", " , signature} , "]"
// set 		= "%" , name , "[" , literal , "]"
// map 		= "#", ["*"] , array
// signature 	= (array | set | map | literal) , ["..."]
// "..." means "0 or more"
// "*" means "needs to be freed". In case of hashmap, key needs to be freed
// TODO update signatures

compiled_t *compile(const char *orig, cmpl_env_t *env) {
  //TODO '.' special symbol
  cmpl_t *const _env = NEW(cmpl_t);
  hashmap_t *const macros = hm_make(), *const sections = hm_make();
  llist_t *const files = ll_make(), *dl = ll_make(), *const history = ll_make();
  
  {
    _env->env = env;
    _env->file = 0;
    _env->symbols = hm_make();
    _env->section = 0;
    helper_section_add(sections, ".text", &_env->section);
    ll_append(files, helper_file_parse(orig, env));
  }

skip:
  while (files->data) {
    llist_t *const file = files->data->value;
      
    if (!file) break;
    free(_env->file);
    _env->file = ll_shift(file);
    _env->f_line = (size_t) ll_shift(file);

    while (file->data) {
      array_t *args = ll_shift(file);
      _env->f_line++;

      if (!history->size) {
	size_t i = 0;
	for (; i < args->size; i++) {
	  const char *word = ar_cget(args, i);
	  if (strchr(word, '\0') - strchr(word, ':') != 1) break;
	  STRMAKE(strlen(word) - 1, symbol, word);
	  struct _lkee *old = hm_get(_env->symbols, symbol);
	  if (!old || ((symbol_t*)old->value)->section == 0) {
	    helper_symbol_set(_env, _env->section->name, symbol, _env->section->address);
	  }
	  free(symbol);
	}
	if (i >= args->size) {
	  ar_free_val(args);
	  continue;
	}
	STRMAKE(strlen(ar_cget(args, i)),word,ar_cget(args, i));
	ar_cutout(args, i+1, args->size);
	
	if (strchr(word, '=')) {
	  {
	    char *delim = strchr(word, '=');
	    STRMAKE(delim-word,symbol,word);
	    helper_symbol_set(_env, "absolute", symbol, helper_expr_parse(_env, delim+1, 0, 0));
	    free(symbol);
	  }
	} else if (*word == '.') {
	  {
	    if (!strcmp(word, ".byte")) 	_parse_int(args, _env, 1);
	    else if (!strcmp(word, ".short")) 	_parse_int(args, _env, 2);
	    else if (!strcmp(word, ".int")) 	_parse_int(args, _env, 4);
	    else if (!strcmp(word, ".long")) 	_parse_int(args, _env, 8);
	    else if (!strcmp(word, ".float"))	_parse_float(args, _env);
	    else if (!strcmp(word, ".double"))	_parse_double(args, _env);
	    else if (!strcmp(word, ".ascii"))	_parse_ascii(args, _env);
	    else if (!strcmp(word, ".asciz"))	_parse_asciz(args, _env);
	    else if (!strcmp(word, ".include")) {
	      _parse_include(args, _env, files);
	      ar_free_val(args);
	      free(word);
	      goto skip;
	    } else if (!strcmp(word, ".err")) 	_parse_err(args, _env);
	    else if (!strcmp(word, ".ifdef")) 	_parse_ifdef(args, _env, history, dl);
	    else if (!strcmp(word, ".ifndef")) 	_parse_ifndef(args, _env, history, dl);
	    else if (!strcmp(word, ".if"))	_parse_if(args, _env, history, dl);
	    else if (!strcmp(word, ".macro")) { _parse_macro(args, _env, dl); ll_prepend(history, "m"); } 
	    else if (!strcmp(word, ".irp")) { 	_parse_irp(args, _env, dl); ll_prepend(history, "rn"); }
	    else if (!strcmp(word, ".irpc")) { 	_parse_irpc(args, _env, dl); ll_prepend(history, "rc"); }
	    else if (!strcmp(word, ".skip")) 	_parse_skip(args, _env);
	    else if (!strcmp(word, ".fill")) 	_parse_fill(args, _env);
	    else if (!strcmp(word, ".align")) 	_parse_align(args, _env);
	    else if (!strcmp(word, ".org")) 	_parse_org(args, _env);
	    else if (!strcmp(word, ".equ")) 	_parse_equ(args, _env);
	    else if (!strcmp(word, ".equiv")) 	_parse_equiv(args, _env);
	    else if (!strcmp(word, ".section")) { 
	      ERR_ONLY(1, ".section",0);
	      if (*((const char*)ARG(0)) != '.') ERR(".section operand must start from '.'",0);
	      helper_section_add(sections, ARG(0), &_env->section);
	    } else if (!strcmp(word, ".data")) { ERR_NONE(".data",0); helper_section_add(sections, ".data", &_env->section); }
	    else if (!strcmp(word, ".text")) { 	 ERR_NONE(".text",0); helper_section_add(sections, ".text", &_env->section); }
	    else if (!strcmp(word, ".code16")) { ERR_NONE(".code16",0); _env->section->bitness = 0; }
	    else if (!strcmp(word, ".code32")) { ERR_NONE(".code32",0); _env->section->bitness = 1; }
	    else if (!strcmp(word, ".comm")) 	_parse_lcomm(args, _env, sections, 1);
	    else if (!strcmp(word, ".lcomm")) 	_parse_lcomm(args, _env, sections, 0);
	    else if (!strcmp(word, ".globl")) 	_parse_globl(args, _env);
	    else ERR("Unknown directive",0);
	  }
	} else {
	  {
	    if (hm_get(macros, word)) {
	      _parse_macro_sub(args, _env, hm_get(macros, word)->value, files);
	      free(word);
	      ar_free_val(args);
	      goto skip;
	    } else if (!strcmp(word, "nop")) _parsei_pure(args, _env, I_NOP);
	    else if (!strcmp(word, "sleep")) _parsei_1reg_imm(args, _env, I_SLEEP, I_SLEEPI);
	    else if (!strcmp(word, "gipc")) _parsei_1reg(args, _env, I_GIPC);
	    else if (!strcmp(word, "mov")) _parsei_2reg_imm(args, _env, I_MOV, I_MOVI);
	    else if (!strcmp(word, "swap")) _parsei_2reg(args, _env, I_SWAP);
	    else if (!strcmp(word, "push")) _parsei_1reg(args, _env, I_PUSH);
	    else if (!strcmp(word, "pop")) _parsei_1reg(args, _env, I_POP);
	    else if (!strcmp(word, "add")) _parsei_2reg_imm(args, _env, I_ADD, I_ADI);
	    else if (!strcmp(word, "sub")) _parsei_2reg_imm(args, _env, I_SUB, I_SBI);
	    else if (!strcmp(word, "mul")) _parsei_2reg(args, _env, I_MUL);
	    else if (!strcmp(word, "div")) _parsei_2reg(args, _env, I_DIV);
	    else if (!strcmp(word, "mod")) _parsei_2reg(args, _env, I_MOD);
	    else if (!strcmp(word, "inc")) _parsei_1reg(args, _env, I_INC);
	    else if (!strcmp(word, "dec")) _parsei_1reg(args, _env, I_DEC);
	    else if (!strcmp(word, "fadd")) _parsei_2reg(args, _env, I_FADD);
	    else if (!strcmp(word, "fsub")) _parsei_2reg(args, _env, I_FSUB);
	    else if (!strcmp(word, "fmul")) _parsei_2reg(args, _env, I_FMUL);
	    else if (!strcmp(word, "fdiv")) _parsei_2reg(args, _env, I_FDIV);
	    else if (!strcmp(word, "i2f")) _parsei_2reg(args, _env, I_I2F);
	    else if (!strcmp(word, "f2i")) _parsei_2reg(args, _env, I_F2I);
	    else if (!strcmp(word, "xor")) _parsei_2reg_imm(args, _env, I_XOR, I_XORI);
	    else if (!strcmp(word, "or")) _parsei_2reg_imm(args, _env, I_OR, I_ORI);
	    else if (!strcmp(word, "and")) _parsei_2reg_imm(args, _env, I_AND, I_ANDI);
	    else if (!strcmp(word, "not")) _parsei_1reg(args, _env, I_NOT);
	    else if (!strcmp(word, "bts")) _parsei_1reg_1imm(args, _env, I_BTS);
	    else if (!strcmp(word, "btr")) _parsei_1reg_1imm(args, _env, I_BTR);
	    else if (!strcmp(word, "btc")) _parsei_1reg_1imm(args, _env, I_BTC);
	    else if (!strcmp(word, "shl")) _parsei_1reg_1imm(args, _env, I_SHL);
	    else if (!strcmp(word, "shr")) _parsei_1reg_1imm(args, _env, I_SHR);
	    else if (!strcmp(word, "rol")) _parsei_1reg_1imm(args, _env, I_ROL);
	    else if (!strcmp(word, "ror")) _parsei_1reg_1imm(args, _env, I_ROR);
	    else if (!strcmp(word, "int")) _parsei_1imm(args, _env, I_INT);
	    else if (!strcmp(word, "jmp")) _parsei_1reg_imm(args, _env, I_JMP, I_JMPI);
	    else if (!strcmp(word, "call")) _parsei_1reg_imm(args, _env, I_CALL, I_CALLI);
	    else if (!strcmp(word, "rjmp")) _parsei_1reg_imm(args, _env, I_RJMP, I_RJMPI);
	    else if (!strcmp(word, "rcall")) _parsei_1reg_imm(args, _env, I_RCALL, I_RCALLI);
	    else if (!strcmp(word, "jae")) _parsei_jcc(args, _env, I_JNC, 0x80);
	    else if (!strcmp(word, "jnae")) _parsei_jcc(args, _env, I_JC, 0x80);
	    else if (!strcmp(word, "jb")) _parsei_jcc(args, _env, I_JC, 0x80);
	    else if (!strcmp(word, "jnb")) _parsei_jcc(args, _env, I_JNC, 0x80);
	    else if (!strcmp(word, "je")) _parsei_jcc(args, _env, I_JC, 0x82);
	    else if (!strcmp(word, "jne")) _parsei_jcc(args, _env, I_JNC, 0x82);
	    else if (!strcmp(word, "jge")) _parsei_jcc(args, _env, I_JC, 0x31);
	    else if (!strcmp(word, "jnge")) _parsei_jcc(args, _env, I_JNC, 0x31);
	    else if (!strcmp(word, "jl")) _parsei_jcc(args, _env, I_JNC, 0x31);
	    else if (!strcmp(word, "jnl")) _parsei_jcc(args, _env, I_JC, 0x31);
	    else if (!strcmp(word, "jc")) _parsei_jcc(args, _env, I_JC, 0x80);
	    else if (!strcmp(word, "jnc")) _parsei_jcc(args, _env, I_JNC, 0x80);
	    else if (!strcmp(word, "jo")) _parsei_jcc(args, _env, I_JC, 0x81);
	    else if (!strcmp(word, "jno")) _parsei_jcc(args, _env, I_JNC, 0x81);
	    else if (!strcmp(word, "js")) _parsei_jcc(args, _env, I_JC, 0x83);
	    else if (!strcmp(word, "jns")) _parsei_jcc(args, _env, I_JNC, 0x83);
	    else if (!strcmp(word, "jz")) _parsei_jcc(args, _env, I_JC, 0x82);
	    else if (!strcmp(word, "jnz")) _parsei_jcc(args, _env, I_JNC, 0x82);
	    else if (!strcmp(word, "movae")) _parsei_movcc(args, _env, I_MOVNC, 0x80);
	    else if (!strcmp(word, "movnae")) _parsei_movcc(args, _env, I_MOVC, 0x80);
	    else if (!strcmp(word, "movb")) _parsei_movcc(args, _env, I_MOVC, 0x80);
	    else if (!strcmp(word, "movnb")) _parsei_movcc(args, _env, I_MOVNC, 0x80);
	    else if (!strcmp(word, "move")) _parsei_movcc(args, _env, I_MOVC, 0x82);
	    else if (!strcmp(word, "movne")) _parsei_movcc(args, _env, I_MOVNC, 0x82);
	    else if (!strcmp(word, "movge")) _parsei_movcc(args, _env, I_MOVC, 0x31);
	    else if (!strcmp(word, "movnge")) _parsei_movcc(args, _env, I_MOVNC, 0x31);
	    else if (!strcmp(word, "movl")) _parsei_movcc(args, _env, I_MOVNC, 0x31);
	    else if (!strcmp(word, "movnl")) _parsei_movcc(args, _env, I_MOVC, 0x31);
	    else if (!strcmp(word, "movc")) _parsei_movcc(args, _env, I_MOVC, 0x80);
	    else if (!strcmp(word, "movnc")) _parsei_movcc(args, _env, I_MOVNC, 0x80);
	    else if (!strcmp(word, "movo")) _parsei_movcc(args, _env, I_MOVC, 0x81);
	    else if (!strcmp(word, "movno")) _parsei_movcc(args, _env, I_MOVNC, 0x81);
	    else if (!strcmp(word, "movs")) _parsei_movcc(args, _env, I_MOVC, 0x83);
	    else if (!strcmp(word, "movns")) _parsei_movcc(args, _env, I_MOVNC, 0x83);
	    else if (!strcmp(word, "movz")) _parsei_movcc(args, _env, I_MOVC, 0x82);
	    else if (!strcmp(word, "movnz")) _parsei_movcc(args, _env, I_MOVNC, 0x82);
	    else if (!strcmp(word, "ret")) _parsei_pure(args, _env, I_RET);
	    else if (!strcmp(word, "in")) _parsei_2reg_imm(args, _env, I_IN, I_INI);
	    else if (!strcmp(word, "out")) _parsei_2reg_imm(args, _env, I_OUT, I_OUTI);
	    else if (!strcmp(word, "crld")) _parsei_2reg(args, _env, I_CRLD);
	    else if (!strcmp(word, "crst")) _parsei_2reg(args, _env, I_CRST);
	    else if (!strcmp(word, "sei")) _parsei_pure(args, _env, I_SEI);
	    else if (!strcmp(word, "cli")) _parsei_pure(args, _env, I_CLI);
	    else if (!strcmp(word, "iret")) _parsei_pure(args, _env, I_IRET);
	    else ERR("Invalid instruction",0);
	  }
	}
	free(word);
	ar_free_val(args);
      } else {
	size_t i = 0;
	for (; i < args->size; i++) {
	  const char *word = ar_cget(args, i);
	  if (strchr(word, '\0') - strchr(word, ':') != 1) break;
	}
	if (i >= args->size) {
	  ar_free_val(args);
	  continue;
	}
	const char *word = ar_cget(args, i);
	
	if ((!strncmp(word, ".if", 3) && ((char*)history->data->value)[0] == 'i') ||
	    (!strcmp(word, ".macro") && ((char*)history->data->value)[0] == 'm') ||
	    (!strncmp(word, ".irp", 4) && ((char*)history->data->value)[0] == 'r')) {
	  ll_prepend(history, history->data->value); // Add frame to history if previous was the same
	} else if (!strcmp(word, ".else") && ((char*)history->data->value)[0] == 'i' && history->size == 1) {
	  _parse_else(_env, history, dl);
	  ar_free_val(args);
	  continue;
	} else if (!strncmp(word, ".end", 4) && ((char*)history->data->value)[0] == word[4] && history->size > 1) {
	  ll_shift(history); // Simply pop frame if there is more than 1 frame
	} else if (!strcmp(word, ".endif") && ((char*)history->data->value)[0] == 'i') {
	  ll_shift(history);
	  _parse_endif(_env, dl, files);
	  dl = ll_make();
	  ar_free_val(args);
	  goto skip;
	} else if (!strcmp(word, ".endm") && ((char*)history->data->value)[0] == 'm') {
	  ll_shift(history);
	  _parse_endm(_env, dl, macros);
	  dl = ll_make();
	  ar_free_val(args);
	  continue;
	} else if (!strcmp(word, ".endr") && ((char*)history->data->value)[0] == 'r') {
	  _parse_endr(_env, ll_shift(history), dl, files);
	  ll_free(dl);
	  dl = ll_make();
	  ar_free_val(args);
	  goto skip;
	}
	ll_append(dl, args);
      }
    }

    ll_shift(files);
    ll_free(file);
  }

  compiled_t *result = NEW(compiled_t);
  {
    llist_t *s_macros = hm_free_to(macros);
    while (s_macros->data) {
      llist_t *entry = ll_shift(s_macros);
      free(ll_shift(entry));
      llist_t *macro = ll_shift(entry);
      ll_free_val(ll_shift(macro));
      helper_file_free(macro);
      ll_free_val(entry);
    }
    ll_free(s_macros);

    for (size_t i=0; i < sections->capacity; i++) {
      for (struct _lkee *entry = sections->buckets[i]; entry; entry = entry->next) {
	_section_t *old = entry->value;
	section_t *new = NEW(section_t);
	entry->value = new;
	new->expr = old->expr;
	new->size = old->address;
	uint8_t *product = emalloc(new->size);
	new->data = product;
	while(old->data->data) {
	  llist_t *part = ll_shift(old->data);
	  size_t len = (size_t) ll_shift(part);
	  memcpy(product, part->data->value, len);
	  product += len;
	  ll_free_val(part);
	}
	
	ll_free(old->data);
	free(old);
      }
    }

    ll_free(files);
    ll_free_val(dl);
    ll_free(history);
    result->symbols = _env->symbols;
    result->sections = sections;
    free(_env->file);
    free(_env);
  }
  return result;
}

/*char *link(llist_t *data, cmpl_env_t *env) {
  llist_t *linker = helper_file_lex(ll_shift(data), env);
  _env->file = ll_shift(linker);
  _env->f_line = (size_t) ll_shift(linker);
  while (linker->data) {
    llist_t *const line = ll_shift(linker);
    _env->f_line++;
    char *word = ll_shift(line);
    while (word && strchr(word, '\0') - strchr(word, ':') == 1) {
      STRMAKE(strlen(word) - 1, symbol, word);
      free(word);
      struct _lkee *old = hm_get(_env->symbols, symbol);
      if (!old || ((symbol_t*)old->value)->section == 0) {
	helper_symbol_set(_env, _env->section->name, symbol, _env->section->address);
      }
      free(symbol);
      word = ll_shift(line);
    }
    if (!word) {
      ll_free_val(line);
      continue;
    }
    array_t *args = ll_free_to(line);

    if (strchr(word, '=')) {
      {
	char *delim = strchr(word, '=');
	STRMAKE(delim-word,symbol,word);
	helper_symbol_set(_env, "absolute", symbol, helper_symbol_expr(_env, delim+1, 0, 0));
	free(symbol);
      }
    } else if (*word == '.') {
      {
	if (!strcmp(word, ".byte")) 	_parse_int(args, _env, 1);
	else ERR("Unknown directive",0);
      }
    } else {
      {
	if (!strcmp(word, "nop")) _parsei_pure(args, _env, I_NOP);
	else ERR("Invalid instruction",0);
      }
    }
    free(word);
    ar_free_val(args);
  }
  ll_free(linker);
}*/

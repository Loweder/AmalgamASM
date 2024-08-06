#include "aasm/internal/compile.h"

static uint8_t _is_space(char datum) {
  return datum == ' ' || datum == '\t' || datum == '\n' || datum == ',' || datum == '\0';
}
static uint8_t _is_allowed(char datum) {
  return (datum >= '0' && datum <= '9') || (datum >= 'a' && datum <= 'z') || (datum >= 'A' && datum <= 'z') || datum == '_' ||datum == '.';
}

llist_t *helper_file_parse(const char *o_name, cmpl_env_t *env) {
#define LINE_ADD { line++; ll_append(result, ll_free_to(r_line)); r_line = ll_make(); }
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
    const char *end;
    if (*data == '"') {
      end = strchr(data+1, '"');
      while (end && *(end-1) == '\\') end = strchr(end+1, '"');
      if (!end) return 0;
      end++;
    } else {
      end = data;
      while (!_is_space(*end)) end++;
    }

    if (!end) LOC_ERR(o_name, "Lexer error: expected \" got EOF",0);

    STRMAKE(end-data, word, data);
    ll_append(r_line, word);

    data = end;
  }
  ll_append(result, ll_free_to(r_line));

  free((void*)o_data);
  return result;
#undef LINE_INC
#undef LINE_ADD
}
void helper_file_free(llist_t *file) {
  {
    free(ll_shift(file));
    ll_shift(file);
  }
 
  E_FOR(entry, file->data) {
    ar_free_val(entry->value);
  }
  ll_free(file);
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
	if (!strcmp(expr->lit.literal, ".")) return _env->section->address;
	else if (!hm_get(_env->symbols, expr->lit.literal))
	  return strtoll(expr->lit.literal, 0, 0);
	symbol_t *symbol = hm_get(_env->symbols, expr->lit.literal)->value;
	if (strcmp(symbol->section, "absolute")) {
	  if (!callback)
	    ERR("Expression parser: requires absolute operands",0)
	  else *callback = 1;
	} else {
	  return (size_t) symbol->value;
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

size_t helper_expr_parse(cmpl_t *_env, const char *expr, size_t offset, size_t size) {
  expr_t *lexed = _lex_expr(expr, _env);
  if (!lexed) return 0;
  size_t callback = 0;
  size_t result = _parse_expr(lexed, _env, size ? &callback : 0);
  if (callback) {
    expr_info_t *call = NEW(expr_info_t);
    call->address = _env->section->address + offset;
    call->size = size;
    STRCPY(strlen(expr), call->expression, expr);
    return 0;
  }
  _free_expr(lexed);
  return result;
}
void helper_section_add(hashmap_t *sections, const char *name, _section_t** current) {
  if (hm_get(sections, name)) {
    *current = hm_get(sections, name)->value;
  } else {
    _section_t *section = NEW(_section_t);
    section->address = 0;
    section->bitness = *current ? (*current)->bitness : 1;
    section->data = ll_make();
    section->expr = ll_make();
    STRCPY(strlen(name),section->name,name);
    hm_put(sections, section->name, section);
    *current = section;
  }
}

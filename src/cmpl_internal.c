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
    } else if (*data == '[') {
      end = strchr(data+1, ']');
      while (end && *(end-1) == '\\') end = strchr(end+1, ']');
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

static uint8_t _parse_prec(char ch) {
  switch (ch) {
    case '!': case '_': case '~':
      return 4;
    case '*': case '/': case '%':
      return 3;
    case '<': case '>':
      return 2;
    case '&': case '|': case '^':
      return 1;
    case '+': case '-': default:
      return 0;
  }
}
static size_t _parse_lit(const char *literal, cmpl_t *_env, uint8_t *callback) {
  if (!strcmp(literal, ".")) return _env->section->address;
  else if (literal[0] >= '0' && literal[0] <= '9')
    return strtoll(literal, 0, 0);
  if (!hm_get(_env->symbols, literal)) {
    *callback = 1;
    return 0;
  }
  symbol_t *symbol = hm_get(_env->symbols, literal)->value;
  if (strcmp(symbol->section, "absolute")) {
    *callback = 1;
    return 0;
  } else {
    return (size_t) symbol->value;
  }
}
static void _parse_op(llist_t *result, char op, cmpl_t *_env, uint8_t *callback) {
  if (result->size < 1) ERR("Expression parser: not enough operands for operator",);
  uint8_t l_callback = 0;
  char *val1 = ll_shift(result), *res = 0;
  size_t lit1 = _parse_lit(val1, _env, &l_callback);
  *callback |= l_callback;
  if (l_callback) {
    switch (op) {
      case '_':
      case '!':
      case '~':
	{
	  size_t len = strlen(val1);
	  res = emalloc(len+2);
	  snprintf(res, len+2, "%c%s", op == '_' ? '-' : op, val1);
	  free(val1);
	  ll_prepend(result, res);
	  return;
	}
    }
  } else {
    size_t product;
    switch (op) {
      case '_': product = -lit1; goto op1;
      case '~': product = ~lit1; goto op1;
      case '!': product = !lit1;
op1:
		res = emalloc(22);
		snprintf(res, 22, "%ld", product);
		free(val1);
		ll_prepend(result, res);
		return;
    }
  }
  if (result->size < 1) ERR("Expression parser: not enough operands for operator",);
  char *val2 = ll_shift(result);
  size_t lit2 = _parse_lit(val2, _env, &l_callback);
  *callback |= l_callback;
  if (l_callback) {
    size_t len = strlen(val1), len2 = strlen(val2);
    res = emalloc(len+len2+4);
    snprintf(res, len+len2+4, "(%s%c%s)", val1, op, val2);
    free(val1);
    free(val2);
    ll_prepend(result, res);
    return;
  } else {
    size_t product;
    switch (op) {
      case '*': product = lit1*lit2; goto op2;
      case '/':
		if (!lit2) ERR("Expression parser: division by zero",);
		product = lit1/lit2; goto op2;
      case '%':
		if (!lit2) ERR("Expression parser: division by zero",);
		product = lit1%lit2; goto op2;
      case '<': product = lit1<<lit2; goto op2;
      case '>': product = lit1>>lit2; goto op2;
      case '&': product = lit1&lit2; goto op2;
      case '|': product = lit1|lit2; goto op2;
      case '^': product = lit1^lit2; goto op2;
      case '+': product = lit1+lit2; goto op2;
      case '-': product = lit1-lit2; 
op2:
		res = emalloc(22);
		snprintf(res, 22, "%ld", product);
		free(val1);
		ll_prepend(result, res);
		return;
    }
  }
}
static char *_parse_expr(const char *expr, cmpl_t *_env, uint8_t *callback) { // Uses modified Shunting Yard algorithm
  llist_t *result = ll_make(), *stack = ll_make();
  uint8_t last_val = 0;
  while(*expr) {
    while(_is_space(*expr) && *expr != '\0') expr++;
    const char *start = expr;
    while(_is_allowed(*expr)) expr++;
    if (start != expr) {
      STRMAKE(expr-start, val, start);
      ll_prepend(result, val);
      last_val = 1;
      continue;
    } else if (*expr == '(') {
      ll_prepend(stack, (void*)'('); 
      last_val = 0;
    } else if (*expr == ')') {
      char op = (size_t) ll_shift(stack);
      while (op != '(') {
	if (!stack->size) ERR("Expression parser: no opening brackets",0);
	_parse_op(result, op, _env, callback);
	op = (size_t) ll_shift(stack);
      }
      last_val = 0;
    } else if (!last_val) {
      switch (*expr) {
	case '+': // Ignore
	  break;
	case '-': // Replace with unique
	  ll_prepend(stack, (void*)'_');
	  break;
	case '~':
	case '!': // Push as-is
	  ll_prepend(stack, (void*)(size_t)*expr);
	  break;
	default:
	  ERR("Expression parser: illegal unary operator",0);
      }
      last_val = 0;
    } else {
      while (stack->size && stack->data->value != (void*)'(') {
	if (_parse_prec((size_t)stack->data->value) < _parse_prec(*expr)) break;
	_parse_op(result, (size_t)ll_shift(stack), _env, callback);
      }
      if ((*expr == '<' || *expr == '>') && *expr == *(expr+1)) expr++;
      ll_prepend(stack, (void*)(size_t)*expr);
      last_val = 0;
    }
    expr++;
  }
  while (stack->size) {
    char op = (size_t) ll_shift(stack);
    if (op == '(') ERR("Expression parser: no closing brackets",0);
    _parse_op(result, op, _env, callback);
  }
  ll_free(stack);
  if (result->size != 1) ERR("Expression parser: unknown error in the equation",0);

  char *res = ll_shift(result);
  ll_free(result);
  return res;
}
size_t helper_expr_parse(cmpl_t *_env, const char *expr, size_t offset, size_t size) {
  uint8_t callback = 0;
  char *result = _parse_expr(expr, _env, &callback);
  if (!result) return 0;
  if (callback) {
    if (!size) ERR("Expression parser: all symbols must be absolute",0);
    expr_info_t *call = NEW(expr_info_t);
    call->address = _env->section->address;
    call->offset = offset;
    call->size = size;
    call->expression = result;
    ll_append(_env->section->expr, call);
    return 0;
  }
  size_t value = strtoll(result, 0, 0);
  free(result);
  return value;
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

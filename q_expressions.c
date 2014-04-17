#include "mpc.h"

#ifdef _WIN32

static char buffer[2048];
char *readline (char *prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char *cpy = malloc(strlen(buffer)+1);
  strncpy(cpy, buffer, sizeof(buffer)-1);
  cpy[strlen(cpy)-1] = ['\0'];
}

void add_history(char *unused);

#else

#include <editline/readline.h>
/* #include <editline/history.h> OSX 10.9 does not need this */

#endif

/* possible lval types */
enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR };

/* macros */
#define LASSERT(args, cond, err) \
  if (!(cond)) \
    { lval_del(args); return lval_err(err); }

/* lval struct */
typedef struct lval {
  int type;
  double num;
  char *err; /* strings this time */
  char *sym;
  int count;
  struct lval **cell; /* list of *lval */
} lval;

/* number type lval */
lval *lval_num(double x) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
  return v;
}

/* error type lval */
lval *lval_err(char *m) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_ERR;
  v->err = malloc(strlen(m)+1);
  strcpy(v->err, m);
  return v;
}

/* symbol type lval */
lval *lval_sym(char *s) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(s)+1);
  strcpy(v->sym, s);
  return v;
}

lval *lval_sexpr(void) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

lval *lval_qexpr(void) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

/* release allocated memory after struct usage */
void lval_del(lval *v) {
  switch (v->type) {
    case LVAL_NUM: break; /* not malloc'ed */
    case LVAL_ERR: free(v->err); break;
    case LVAL_SYM: free(v->sym); break;
    /* if sexpr or sexpr delete all elements inside */
    case LVAL_SEXPR:
    case LVAL_QEXPR:
      for (int i=0; i < v->count; i++) {
        lval_del(v->cell[i]); /* recursive delete */
      }
      free(v->cell);
      break;
  }
  free(v);
}

/* add element to s-expression */
lval *lval_add(lval *v, lval *x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  v->cell[v->count-1] = x;
  return v;
}

/* remove element from an S-expression, does not delete input */
lval *lval_pop(lval *v, int i) {
  lval *x = v->cell[i]; /* find the element */

  /* shift the memory following the item at "i" over the top of it */
  memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count-i-1));

  v->count--; /* decrease the count by one */

  /* reallocate the memory used, footprint has decreased */
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  return x;
}

/* variation of lval_pop, deletes input */
lval *lval_take(lval *v, int i) {
  lval *x = lval_pop(v, i);
  lval_del(v);
  return x;
}

void lval_print(lval *v);

void lval_expr_print(lval *v, char open, char close) {
  putchar(open);
  for (int i=0; i < v->count; i++) {
    lval_print(v->cell[i]); /* print value */
    /* don't print trailing space if last element */
    if (i != (v->count-1)) {
      putchar(' ');
    }
  }
  putchar(close);
}

/* print an "lval" */
void lval_print(lval *v) {
  switch (v->type) {
    /* just print the numeric value */
    case LVAL_NUM:   printf("%f", v->num); break;
    case LVAL_ERR:   printf("Error: %s", v->err); break;
    case LVAL_SYM:   printf("%s", v->sym); break;
    case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
    case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
  }
}

/* put newline after lval printing */
void lval_println(lval *v) { lval_print(v); putchar('\n'); }

/* evaluation and error checking */
lval *builtin_op(lval *a, char *op) {
  /* ensure arguments are numbers */
  for (int i=0; i < a->count; i++) {
    if (a->cell[i]->type != LVAL_NUM) {
      lval_del(a);
      return lval_err("Cannot operate on non-number!");
    }
  }
  lval *x = lval_pop(a, 0); /* pop the first element */

  /* if no arguments and sub -> negation */
  if ((strcmp(op, "-") == 0) && a->count == 0) { x->num = -x->num; }

  /* go thru remaining elements */
  while (a->count > 0) {
    lval *y = lval_pop(a, 0); /* pop next element */

    /* perform operation */
    if (strcmp(op, "+") == 0) { x->num += y->num; }
    if (strcmp(op, "-") == 0) { x->num -= y->num; }
    if (strcmp(op, "*") == 0) { x->num *= y->num; }
    if (strcmp(op, "/") == 0) {
      if (y->num == 0) {
        lval_del(x);
        lval_del(y);
        x = lval_err("Division by zero!");
        break;
      } x->num /= y->num; }
    if (strcmp(op, "%") == 0) { x->num = (int) x->num % (int) y->num; }
    if (strcmp(op, "^") == 0) { x->num = pow(x->num, y->num); }

    lval_del(y); /* delete element now finished with */
  }
  /* delete input expression and return result */
  lval_del(a);
  return x;
}

lval *builtin_head(lval *a) {
  /* lots of error checking */
  LASSERT(a, (a->count == 1),                  "Function 'head' passed too many arguments!");
  LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'head' passed incorrect type!");
  LASSERT(a, (a->cell[0]->count != 0),         "Function 'head' passed {}!");

  /* if everything ok, take the first argument */
  lval *v = lval_take(a, 0);

  /* delete all elements that are not head and return */
  while(v->count > 1) {
    lval_del(lval_pop(v, 1));
  }
  return v;
}

lval *builtin_tail(lval *a) {
  /* error checks */
  LASSERT(a, (a->count == 1),                  "Function 'tail' passed too many arguments!");
  LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'tail' passed incorrect type!");
  LASSERT(a, (a->cell[0]->count != 0),         "Function 'tail' passed {}!");

  /* take first argument */
  lval *v = lval_take(a, 0);

  /* delete first element and return */
  lval_del(lval_pop(v,0));
  return v;
}

lval *builtin_list(lval *a) {
  /* just convert S-expression to Q-expression */
  a->type = LVAL_QEXPR;
  return a;
}

lval *lval_eval(lval *v);

lval *builtin_eval(lval *a) {
  /* convert Q-expression to S-expression and eval it */
  LASSERT(a, (a->count == 1),                  "Function 'eval' passed too many arguments!");
  LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'eval' passed incorrect type!");

  lval *x = lval_take(a, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(x);
}

lval *lval_join(lval *x, lval *y) {
  /* for each cell in 'y' add it to 'x' */
  while (y->count) {
    x = lval_add(x, lval_pop(y, 0));
  }

  /* delete ampty y and return joined x */
  lval_del(y);
  return x;
}

/* join Q-expressions */
lval *builtin_join(lval *a) {
  /* check for correct type */
  for (int i=0; i < a->count; i++) {
    LASSERT(a, (a->cell[i]->type == LVAL_QEXPR), "Function 'join' passed incorrect type!");
  }

  lval *x = lval_pop(a, 0);

  while (a->count) {
    x = lval_join(x, lval_pop(a, 0));
  }

  lval_del(a);
  return x;
}

/* map different builtin functions behind this one function */
lval *builtin(lval *a, char *func) {
  if (strcmp("list", func) == 0) { return builtin_list(a); }
  if (strcmp("head", func) == 0) { return builtin_head(a); }
  if (strcmp("tail", func) == 0) { return builtin_tail(a); }
  if (strcmp("join", func) == 0) { return builtin_join(a); }
  if (strcmp("eval", func) == 0) { return builtin_eval(a); }
  if (strstr("+-/*%^", func)) { return builtin_op(a, func); }
  lval_del(a);
  return lval_err("Uknnown function!");
}

lval *lval_eval_sexpr(lval *v) {
  /* evaluate children */
  for (int i=0; i < v->count; i++) {
    v->cell[i] = lval_eval(v->cell[i]);
  }

  /* error checking */
  for (int i=0; i < v->count; i++) {
    if (v->cell[i]->type == LVAL_ERR) { return lval_take(v,i); }
  }

  /* empty expression */
  if (v->count == 0) { return v; }

  /* single expression */
  if (v->count == 1) { return lval_take(v, 0); }

  /* ensure first element is symbol */
  lval *f = lval_pop(v, 0);
  if (f->type != LVAL_SYM) {
    lval_del(f);
    lval_del(v);
    return lval_err("S-expression does not start with symbol!");
  }

  /* call builtin with operator */
  lval *result = builtin(v, f->sym);
  lval_del(f);
  return result;
}

lval *lval_eval(lval *v) {
  /* evaluate s-expressions */
  if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(v); }
  return v; /* all other types remain the same */
}

lval *lval_read_num(mpc_ast_t *t) {
  errno = 0;
  double x = strtod(t->contents, NULL);
  return errno != ERANGE ? lval_num(x) : lval_err("Invalid number");
}

lval *lval_read(mpc_ast_t *t) {
  /* if symbol or number return conversion to that type */
  if (strstr(t->tag, "number")) { return lval_read_num(t); }
  if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

  /* if root (>) or sexpr then create empty list */
  lval *x = NULL;
  if(strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
  if(strstr(t->tag, "sexpr"))  { x = lval_sexpr(); }
  if(strstr(t->tag, "qexpr"))  { x = lval_qexpr(); }

  /* check for valid expression */
  for (int i=0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
    if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
    if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
    x = lval_add(x, lval_read(t->children[i]));
  }
  return x;
}

int main(int argc, char **argv) {
  /* setup grammar */
  mpc_parser_t *Number = mpc_new("number");
  mpc_parser_t *Symbol = mpc_new("symbol");
  mpc_parser_t *Sexpr = mpc_new("sexpr");
  mpc_parser_t *Qexpr = mpc_new("qexpr");
  mpc_parser_t *Expr = mpc_new("expr");
  mpc_parser_t *Lispy = mpc_new("lispy");

  mpca_lang(MPC_LANG_DEFAULT,
          " \
          number    : /-?[0-9]+(\\.)?([0-9]+)?/; \
          symbol    : \"list\" | \"head\" | \"tail\" | \"join\" | \"eval\" | '+' | '-' | '*' | '/' | '%' | '^' ; \
          sexpr     : '(' <expr>* ')' ; \
          qexpr     : '{' <expr>* '}' ; \
          expr      : <number> | <symbol> | <sexpr> | <qexpr> ; \
          lispy     : /^/ <expr>* /$/; \
          ",
          Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

  puts("Lispy version 0.0.0.0.5");
  puts("Press Ctrl+C to Exit\n");

  while(1) {
    char *input = readline("lispy> ");
    add_history(input);

    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      lval *x = lval_eval(lval_read(r.output));
      lval_println(x);
      lval_del(x);
      mpc_ast_delete(r.output);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
    free(input);
  }
  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
  return 0;
}

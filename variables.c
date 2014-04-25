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

/* macros */
#define LASSERT(args, cond, err) if (!(cond)) { lval_del(args); return lval_err(err); }

struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

/* possible lval types */
enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR, LVAL_FUN };

/* Function pointer (http://www.buildyourownlisp.com/chapter11_variables):
  "To get an lval* we dereference lbuiltin and call it with a lenv*
  and lval*. Therefore lbuiltin must be a function pointer that
  takes an lenv* and lval* and returns a lval*. */
typedef lval*(*lbuiltin)(lenv*, lval*);

/* environment struct */
struct lenv {
  int count;
  char **syms;
  lval **vals;
};

/* lval struct */
struct lval {
  int type;
  double num;
  char *err;
  char *sym;
  lbuiltin fun;
  int count;
  lval **cell;
};

/* new environment */
lenv *lenv_new(void) {
  lenv *e = malloc(sizeof(lenv));
  e->count = 0;
  e->syms = NULL;
  e->vals = NULL;
  return e;
}

void lval_del(lval *v);
lval *lval_copy(lval *v);
lval *lval_err(char *m);

/* delete environment */
void lenv_del(lenv *e) {
  for (int i=0; i < e->count; i++) {
    free(e->syms[i]);
    lval_del(e->vals[i]);
  }
  free(e->syms);
  free(e->vals);
  free(e);
}

/* get value from environment */
lval *lenv_get(lenv *e, lval *k) {
  /* look for value in environment */
  for (int i=0; i < e->count; i++) {
    /* check if stored string matches the symbol string */
    if (strcmp(e->syms[i], k->sym) == 0) {
      return lval_copy(e->vals[i]);
    }
  }
  /* if no symbol matches */
  return lval_err("Unbound symbol!");
}

/* add new value to environment */
void lenv_put(lenv *e, lval *k, lval *v) {
  for (int i=0; i < e->count; i++) {
    /* if variable is found remove old and replace with new value */
    if (strcmp(e->syms[i], k->sym) == 0) {
      lval_del(e->vals[i]);
      e->vals[i] = lval_copy(v);
    }
  }

  /* if no existing entry found allocate space for new entry */
  e->count++;
  e->vals = realloc(e->vals, sizeof(lval*) * e->count);
  e->syms = realloc(e->syms, sizeof(char*) * e->count);

  /* copy contents into new location */
  e->vals[e->count-1] = lval_copy(v);
  e->syms[e->count-1] = malloc(strlen(k->sym)+1);
  strcpy(e->syms[e->count-1], k->sym);
}

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

/* function type lval */
lval *lval_fun(lbuiltin func) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_FUN;
  v->fun = func;
  return v;
}

/* copy lvals */
lval *lval_copy(lval *v) {
  lval *x = malloc(sizeof(lval));
  x->type = v->type;

  switch(v->type) {
    /* copy functions and numbers directly */
    case LVAL_FUN: x->fun = v->fun; break;
    case LVAL_NUM: x->num = v->num; break;

    /* copy strings using malloc and strcpy */
    case LVAL_ERR: x->err = malloc(strlen(v->err)+1); strcpy(x->err, v->err); break;
    case LVAL_SYM: x->sym = malloc(strlen(v->sym)+1); strcpy(x->sym, v->sym); break;

    /* copy lists by copying each sub-expression */
    case LVAL_SEXPR:
    case LVAL_QEXPR:
      x->count = v->count;
      x->cell = malloc(sizeof(lval *) * x->count);
      for (int i=0; i < x->count; i++) {
        x->cell[i] = lval_copy(v->cell[i]);
      }
      break;
  }
  return x;
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
lval *builtin_op(lenv *e, lval *a, char *op) {
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

lval *builtin_head(lenv *e, lval *a) {
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

lval *builtin_tail(lenv *e, lval *a) {
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

lval *builtin_list(lenv *e, lval *a) {
  /* just convert S-expression to Q-expression */
  a->type = LVAL_QEXPR;
  return a;
}

lval *lval_eval(lenv *e, lval *v);

lval *builtin_eval(lenv *e, lval *a) {
  /* convert Q-expression to S-expression and eval it */
  LASSERT(a, (a->count == 1),                  "Function 'eval' passed too many arguments!");
  LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'eval' passed incorrect type!");

  lval *x = lval_take(a, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(e, x);
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
lval *builtin_join(lenv *e, lval *a) {
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
lval *builtin(lenv *e, lval *a, char *func) {
  if (strcmp("list", func) == 0) { return builtin_list(e, a); }
  if (strcmp("head", func) == 0) { return builtin_head(e, a); }
  if (strcmp("tail", func) == 0) { return builtin_tail(e, a); }
  if (strcmp("join", func) == 0) { return builtin_join(e, a); }
  if (strcmp("eval", func) == 0) { return builtin_eval(e, a); }
  if (strstr("+-/*%^", func)) { return builtin_op(e, a, func); }
  lval_del(a);
  return lval_err("Uknnown function!");
}

lval* builtin_add(lenv* e, lval* a) { return builtin_op(e, a, "+"); }
lval* builtin_sub(lenv* e, lval* a) { return builtin_op(e, a, "-"); }
lval* builtin_mul(lenv* e, lval* a) { return builtin_op(e, a, "*"); }
lval* builtin_div(lenv* e, lval* a) { return builtin_op(e, a, "/"); }

void lenv_add_builtin(lenv *e, char *name, lbuiltin func) {
  lval *k = lval_sym(name);
  lval *v = lval_fun(func);
  lenv_put(e, k, v);
  lval_del(k);
  lval_del(v);
}

void lenv_add_builtins(lenv *e) {
  lenv_add_builtin(e, "list", builtin_list);
  lenv_add_builtin(e, "head", builtin_head);
  lenv_add_builtin(e, "tail", builtin_tail);
  lenv_add_builtin(e, "eval", builtin_eval);
  lenv_add_builtin(e, "join", builtin_join);
  lenv_add_builtin(e, "+", builtin_add);
  lenv_add_builtin(e, "-", builtin_sub);
  lenv_add_builtin(e, "*", builtin_mul);
  lenv_add_builtin(e, "/", builtin_div);
}

lval *lval_eval_sexpr(lenv *e, lval *v) {
  /* evaluate children */
  for (int i=0; i < v->count; i++) {
    v->cell[i] = lval_eval(e, v->cell[i]);
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
  if (f->type != LVAL_FUN) {
    lval_del(f);
    lval_del(v);
    return lval_err("first element is not a function!");
  }

  /* call function to get result */
  lval *result = f->fun(e, v);
  lval_del(f);
  return result;
}

lval *lval_eval(lenv *e, lval *v) {
  if (v->type == LVAL_SYM) {
    lval *x = lenv_get(e, v);
    lval_del(v);
    return x;
  }
  /* evaluate s-expressions */
  if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(e, v); }
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
          number    : /(-|+)?[0-9]+(\\.)?([0-9]+)?/; \
          symbol    : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ; \
          sexpr     : '(' <expr>* ')' ; \
          qexpr     : '{' <expr>* '}' ; \
          expr      : <number> | <symbol> | <sexpr> | <qexpr> ; \
          lispy     : /^/ <expr>* /$/; \
          ",
          Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

  puts("Lispy version 0.0.0.0.5");
  puts("Press Ctrl+C to Exit\n");

  lenv *e = lenv_new();
  lenv_add_builtins(e);

  while(1) {
    char *input = readline("lispy> ");
    add_history(input);

    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      lval *x = lval_eval(e, lval_read(r.output));
      lval_println(x);
      lval_del(x);
      mpc_ast_delete(r.output);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
    free(input);
  }
  lenv_del(e);
  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
  return 0;
}

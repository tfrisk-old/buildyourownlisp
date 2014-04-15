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
enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR };

/* lval struct */
typedef struct lval {
  int type;
  long num;
  char *err; /* strings this time */
  char *sym;
  int count;
  struct lval **cell; /* list of *lval */
} lval;

/* number type lval */
lval *lval_num(long x) {
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

/* release allocated memory after struct usage */
void lval_del(lval *v) {
  switch (v->type) {
    case LVAL_NUM: break; /* not malloc'ed */
    case LVAL_ERR: free(v->err); break;
    case LVAL_SYM: free(v->sym); break;
    /* if sexpr delete all elements inside */
    case LVAL_SEXPR:
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

lval *lval_read_num(mpc_ast_t *t) {
  errno = 0;
  long x = strtol(t->contents, NULL, 10);
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
    case LVAL_NUM:   printf("%li", v->num); break;
    case LVAL_ERR:   printf("Error: %s", v->err); break;
    case LVAL_SYM:   printf("%s", v->sym); break;
    case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
  }
}

/* put newline after lval printing */
void lval_println(lval *v) { lval_print(v); putchar('\n'); }

int main(int argc, char **argv) {
  /* setup grammar */
  mpc_parser_t *Number = mpc_new("number");
  mpc_parser_t *Symbol = mpc_new("symbol");
  mpc_parser_t *Sexpr = mpc_new("sexpr");
  mpc_parser_t *Expr = mpc_new("expr");
  mpc_parser_t *Lispy = mpc_new("lispy");

  mpca_lang(MPC_LANG_DEFAULT,
          " \
          number    : /-?[0-9]+/; \
          symbol    : '+' | '-' | '*' | '/' | '%' | '^' ; \
          sexpr     : '(' <expr>* ')' ; \
          expr      : <number> | <symbol> | <sexpr> ; \
          lispy     : /^/ <expr>* /$/; \
          ",
          Number, Symbol, Sexpr, Expr, Lispy);

  puts("Lispy version 0.0.0.0.5");
  puts("Press Ctrl+C to Exit\n");

  while(1) {
    char *input = readline("lispy> ");
    add_history(input);

    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      lval *x = lval_read(r.output);
      lval_println(x);
      lval_del(x);
      mpc_ast_delete(r.output);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
    free(input);
  }
  mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Lispy);
  return 0;
}

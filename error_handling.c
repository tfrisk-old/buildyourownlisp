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

/* possible error types */
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

/* possible lval types */
enum { LVAL_NUM, LVAL_ERR };

/* lval struct */
typedef struct {
  int type;
  long num;
  int err;
} lval;

/* number type lval */
lval lval_num(long x) {
  lval v;
  v.type = LVAL_NUM;
  v.num = x;
  return v;
}

/* error type lval */
lval lval_err(int x) {
  lval v;
  v.type = LVAL_ERR;
  v.err = x;
  return v;
}

/* print an "lval" */
void lval_print(lval v) {
  switch (v.type) {
    /* just print the numeric value */
    case LVAL_NUM: printf("%li", v.num); break;
    case LVAL_ERR:
      /* print custom error message for each error case */
      if (v.err == LERR_DIV_ZERO) { printf("Error: Division By Zero!"); }
      if (v.err == LERR_BAD_OP) { printf("Error: Invalid Operator!"); }
      if (v.err == LERR_BAD_NUM) { printf("Error: Invalid Number!"); }
    break;
  }
}

/* put newline after lval printing */
void lval_println(lval v) { lval_print(v); putchar('\n'); }

/* read operator string to see which operation to perform */
lval eval_op(lval x, char *op, lval y) {
  /* if either value is error return it */
  if (x.type == LVAL_ERR) { return x; }
  if (y.type == LVAL_ERR) { return y; }

  if (strcmp(op, "+") == 0) { return lval_num(x.num + y.num); }
  if (strcmp(op, "-") == 0) { return lval_num(x.num - y.num); }
  if (strcmp(op, "*") == 0) { return lval_num(x.num * y.num); }
  if (strcmp(op, "/") == 0) {
    /* check for division by zero */
    return y.num == 0 ? lval_err(LERR_DIV_ZERO) : lval_num(x.num / y.num); }
  if (strcmp(op, "%") == 0) { return lval_num(x.num % y.num); }
  if (strcmp(op, "^") == 0) { return lval_num(pow(x.num, y.num)); }

  return lval_err(LERR_BAD_OP); /* otherwise unknown operator */
}

lval eval(mpc_ast_t *t) {
  /* check errors in number conversion */
  if (strstr(t->tag, "number")) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
  }

  /* operator is always 2nd child */
  char *op = t->children[1]->contents;

  /* store 3rd child in x */
  lval x = eval(t->children[2]);

  /* iterate remaining children, operator stays the same */
  int i = 3;
  while (strstr(t->children[i]->tag, "expr")) {
    x = eval_op(x, op, eval(t->children[i]));
    i ++;
  }
  return x;
}

/* calculate number of children nodes recursively */
int number_of_nodes(mpc_ast_t *t) {
  if (t->children_num == 0) { return 1; }
  if (t->children_num >= 1) {
    int total = 1;
    for (int i=0; i < t->children_num; i++) {
      total = total + number_of_nodes(t->children[i]);
    }
    return total;
  }
  return 0; /* should not be here */
}

int main(int argc, char **argv) {
  /* setup grammar */
  mpc_parser_t *Number = mpc_new("number");
    mpc_parser_t *Operator = mpc_new("operator");
    mpc_parser_t *Expr = mpc_new("expr");
    mpc_parser_t *Lispy = mpc_new("lispy");

  mpca_lang(MPC_LANG_DEFAULT,
          " \
          number    : /-?[0-9]+/; \
          operator  : '+' | '-' | '*' | '/' | '%' | '^' | '#' ; \
          expr      : <number> | '(' <operator> <expr>+ ')'; \
          lispy     : /^/ <operator> <expr>+ /$/; \
          ",
          Number, Operator, Expr, Lispy);

  puts("Lispy version 0.0.0.0.4");
  puts("Press Ctrl+C to Exit\n");

  while(1) {
    char *input = readline("lispy> ");
    add_history(input);

    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      lval result = eval(r.output);
      lval_println(result);
      mpc_ast_delete(r.output);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
    free(input);
  }
  mpc_cleanup(4, Number, Operator, Expr, Lispy);
  return 0;
}

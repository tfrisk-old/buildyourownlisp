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

/* read operator string to see which operation to perform */
long eval_op(long x, char *op, long y) {
  /* printf("eval_op(%li, %s, %li)\n", x, op, y); */
  if (strcmp(op, "+") == 0) { return x + y; }
  if (strcmp(op, "-") == 0) { return x - y; }
  if (strcmp(op, "*") == 0) { return x * y; }
  if (strcmp(op, "/") == 0) { return x / y; }
  if (strcmp(op, "%") == 0) { return x % y; }
  if (strcmp(op, "^") == 0) { return pow(x, y); }
  return 0;
}

long eval(mpc_ast_t *t) {
  /* if tag is number just return it */
  if (strstr(t->tag, "number")) {
    return atoi(t->contents);
  }

  /* operator is always 2nd child */
  char *op = t->children[1]->contents;

  /* store 3rd child in x */
  long x = eval(t->children[2]);

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
          operator  : '+' | '-' | '*' | '/' | '%' | '^' ; \
          expr      : <number> | '(' <operator> <expr>+ ')'; \
          lispy     : /^/ <operator> <expr>+ /$/; \
          ",
          Number, Operator, Expr, Lispy);

  puts("Lispy version 0.0.0.0.3");
  puts("Press Ctrl+C to Exit\n");

  while(1) {
    char *input = readline("lispy> ");
    add_history(input);

    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      long result = eval(r.output);
      printf("%li\n", result);
      /* printf("Children nodes: %d\n", number_of_nodes(r.output)); */
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

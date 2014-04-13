/*
http://www.buildyourownlisp.com/chapter6_parsing
*/

#include <stdlib.h>
#include <stdio.h>
#include <editline/readline.h>
#include "mpc.h"

int main(int argc, char **argv) {
  /* version and exit info */
  puts("Lispy Version 0.0.0.0.3");
  puts("Press Ctrl+C to Exit");

  mpc_parser_t *Number = mpc_new("number");
  mpc_parser_t *Operator = mpc_new("operator");
  mpc_parser_t *Expr = mpc_new("expr");
  mpc_parser_t *Lispy = mpc_new("lispy");

  mpc_result_t r;

  mpca_lang(MPC_LANG_DEFAULT,
            " \
            number    : /(-|+)?[0-9]+/ ; \
            operator  : '+' | \"add\" | '-' | \"sub\" | '*' | \"mul\" | '/' | \"div\" ; \
            expr      : <number> | '(' <operator> <expr>+ ')' ; \
            lispy     : /^/ <operator> <expr>+ /$/ ; \
            ",
            Number, Operator, Expr, Lispy);

  /* Do some parsing */
  while(1) {
    /* display prompt and get input */
    char *input = readline("mylisp> ");
    /* add input to history */
    add_history(input);

    /* attempt to parse the user input */
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      /* on success print the AST */
      mpc_ast_print(r.output);
      mpc_ast_delete(r.output);
    } else {
      /* otherwise print the error */
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    /* free retrieved input */
    free(input);
  }

  mpc_cleanup(4, Number, Operator, Expr, Lispy);

  return 0;
}

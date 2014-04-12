/*
http://www.buildyourownlisp.com/chapter4_interactive_prompt
*/

#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>

int main(int argc, char **argv) {
  /* version and exit info */
  puts("Lispy Version 0.0.0.0.2");
  puts("Press Ctrl+C to Exit");

  while(1) {
    /* display prompt and get input */
    char *input = readline("mylisp> ");

    /* add input to history */
    add_history(input);

    /* echo input back to user */
    printf("Echo: %s\n", input);

    /* free retrieved input */
    free(input);
  }

  return 0;
}

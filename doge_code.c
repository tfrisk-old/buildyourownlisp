#include "mpc.h"

int main(int argc, char **argv) {
  /* Build a new parser 'Adjective' to recognize descriptions */
  mpc_parser_t *Adjective = mpc_or(4,
    mpc_sym("wow"), mpc_sym("many"),
    mpc_sym("so"), mpc_sym("such"));

  /* Build a new parser 'Noun' to recognize things */
  mpc_parser_t *Noun = mpc_or(5,
    mpc_sym("lisp"), mpc_sym("language"),
    mpc_sym("c"), mpc_sym("book"),
    mpc_sym("build"));

  /* define Phrase */
  mpc_parser_t *Phrase = mpc_and(2, mpcf_strfold, Adjective, Noun, free);

  /* define Doge as many phrases */
  mpc_parser_t *Doge = mpc_many(mpcf_strfold, Phrase);

  /* Do some parsing */

  mpc_delete(Doge);

  return 0;
}

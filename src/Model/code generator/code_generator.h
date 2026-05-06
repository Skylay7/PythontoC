/* code_generator.h */

#ifndef CODE_GENERATOR_H
#define CODE_GENERATOR_H

#include "../parser/ast_ds.h"
#include "../errors/errors.h"

/* Walk the typed AST and produce C source text.
   Returns a heap-allocated string the caller must free, or NULL on failure. */
char *generate_code(const ASTNode *root, ErrorList *errors);

#endif

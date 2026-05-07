/* code_generator.h */

#ifndef CODE_GENERATOR_H
#define CODE_GENERATOR_H

#include "../parser/ast_ds.h"
#include "../errors/errors.h"
#include "../Symantic analayzer/symantic_analyzer.h"

/* Walk the typed AST and produce C source text.
   global_scope is the scope tree produced by semantic_analyze; it is used to track C declarations
   and must remain valid for the duration of the call.
   Returns a heap-allocated string the caller must free, or NULL on failure. */
char *generate_code(const ASTNode *root, SymbolTable *global_scope, ErrorList *errors);

#endif

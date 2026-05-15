/* code_generator.h */

#ifndef CODE_GENERATOR_H
#define CODE_GENERATOR_H

#include "../parser/ast_ds.h"
#include "../errors/errors.h"
#include "../Symantic analayzer/symantic_analyzer.h"

/* Dynamic string buffer — grows by doubling when full. */
typedef struct
{
   char *data; /* heap-allocated character buffer */
   int len;    /* number of characters written (excluding null terminator) */
   int cap;    /* current allocated capacity */
} CodeBuf;

/* State threaded through the entire code generation walk. */
typedef struct
{
   CodeBuf *buf;               /* output buffer being written to (fn_buf or main_buf) */
   SymbolTable *current_scope; /* mirrors the semantic analyzer's scope tree */
   int indent;                 /* current indentation level in emitted C */
   int tmp_counter;            /* counter for unique __tmp_N temp variable names */
   ErrorList *errors;          /* shared error list; not owned */
} CodeGen;

/* Walk the typed AST and produce C source text.
   global_scope is the scope tree produced by semantic_analyze; it is used to track C declarations
   and must remain valid for the duration of the call.
   Returns a heap-allocated string the caller must free, or NULL on failure. */
char *generate_code(const ASTNode *root, SymbolTable *global_scope, ErrorList *errors);

#endif

/* symantic_analyzer.h
 *
 * Semantic analyzer — walks the AST produced by the parser, infers types
 * for every node, and enforces scope rules via a linked chain of symbol
 * tables (one per block / function body).
 *
 * Public API:
 *   semantic_analyze(root, errors)
 *       Walk the tree, fill inferred_type on every node, populate the
 *       global symbol table.  All errors go to the shared ErrorList.
 *       Returns the filled root (same pointer) or NULL on fatal failure.
 */

#ifndef SYMANTIC_ANALYZER_H
#define SYMANTIC_ANALYZER_H

#include "../parser/ast_ds.h"
#include "../errors/errors.h"

/* ------------------------------------------------------------------ */
/* Symbol table                                                         */
/* ------------------------------------------------------------------ */

#define SYMBOL_TABLE_SIZE  64   /* slots per scope level (power of 2)  */
#define MAX_SYMBOL_NAME   256   /* maximum identifier length            */

typedef struct SymbolEntry
{
    char         name[MAX_SYMBOL_NAME];
    SemanticType type;
    int          occupied;
} SymbolEntry;

typedef struct SymbolTable
{
    SymbolEntry        entries[SYMBOL_TABLE_SIZE];
    struct SymbolTable *parent; /* enclosing scope — NULL at global level */
} SymbolTable;

/* ------------------------------------------------------------------ */
/* Analyzer context                                                     */
/* ------------------------------------------------------------------ */

typedef struct
{
    SymbolTable *current; /* scope currently being analysed             */
    SymbolTable  global;  /* global (root) scope                        */
    ErrorList   *errors;  /* shared error list — never NULL             */
} SemanticAnalyzer;

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

/* Walk root, infer types, enforce scopes.
   Returns root with inferred_type filled on every node. */
ASTNode *semantic_analyze(ASTNode *root, ErrorList *errors);

#endif

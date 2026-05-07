/* symantic_analyzer.h */

#ifndef SYMANTIC_ANALYZER_H
#define SYMANTIC_ANALYZER_H

#include "../parser/ast_ds.h"
#include "../errors/errors.h"

#define SYMBOL_TABLE_SIZE 256
#define MAX_SYMBOL_NAME 256
#define MAX_CHILD_SCOPES 64

typedef struct SymbolEntry
{
    char name[MAX_SYMBOL_NAME];
    SemanticType type;
    int occupied;
    int declared_in_c; // set by code generator on first C declaration
} SymbolEntry;

typedef struct SymbolTable
{
    SymbolEntry entries[SYMBOL_TABLE_SIZE];
    struct SymbolTable *parent;                     // NULL at global scope
    struct SymbolTable *children[MAX_CHILD_SCOPES]; // child scopes in creation order
    int child_count;
    int next_child; // traversal cursor for the code generator
} SymbolTable;

typedef struct
{
    SymbolTable *current; // innermost active scope
    SymbolTable *global;  // heap-allocated root scope
    ErrorList *errors;
    SemanticType current_fn_return; // accumulates return type of function being analyzed
} SemanticAnalyzer;

/* Walks the AST, infers types on every node, and checks scopes.
   Returns the same root pointer with inferred_type filled in.
   Writes the heap-allocated global scope to *out_scope (caller must free via symbol_table_free_tree). */
ASTNode *semantic_analyze(ASTNode *root, ErrorList *errors, SymbolTable **out_scope);

/* Looks up name in this scope only — does NOT walk the parent chain.
   Returns a pointer to the live SymbolEntry so the caller can mutate declared_in_c, or NULL if not found. */
SymbolEntry *symbol_table_lookup_local(SymbolTable *table, const char *name);

/* Recursively frees a scope tree produced by semantic_analyze. */
void symbol_table_free_tree(SymbolTable *table);

#endif

/* symantic_analyzer.h */

#ifndef SYMANTIC_ANALYZER_H
#define SYMANTIC_ANALYZER_H

#include "../parser/ast_ds.h"
#include "../errors/errors.h"

#define SYMBOL_TABLE_SIZE 64
#define MAX_SYMBOL_NAME 256

typedef struct SymbolEntry
{
    char name[MAX_SYMBOL_NAME];
    SemanticType type;
    int occupied; // flag to indicate if this slot is used
} SymbolEntry;

typedef struct SymbolTable
{
    SymbolEntry entries[SYMBOL_TABLE_SIZE];
    struct SymbolTable *parent; // NULL at global scope
} SymbolTable;

typedef struct
{
    SymbolTable *current; // points to the innermost active scope
    SymbolTable global;   // the root scope, always alive for the duration of analysis
    ErrorList *errors;
    SemanticType current_fn_return; // accumulates return type of the function being analyzed
} SemanticAnalyzer;

/* Walks the AST, infers types on every node, and checks scopes.
   Returns the same root pointer with inferred_type filled in. */
ASTNode *semantic_analyze(ASTNode *root, ErrorList *errors);

#endif

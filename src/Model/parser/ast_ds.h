/* ast_ds.h
 *
 * ASTNode — the single node type that makes up the Abstract Syntax Tree.
 *
 * Every node has:
 *   type          — what syntactic construct this node represents
 *   value         — optional textual payload (identifier name, literal text)
 *   inferred_type — semantic type, filled in by the semantic-analysis stage
 *   children      — ordered dynamic array of child nodes (owned by this node)
 *   line, column  — source position of the construct, for error messages
 */

#ifndef AST_DS_H
#define AST_DS_H

#include "node_types.h"
#include "../Symantic analayzer/semantic_types.h"

/* ------------------------------------------------------------------ */
/* ASTNode                                                             */
/* ------------------------------------------------------------------ */

#define AST_MAX_VALUE 256  /* maximum length of the value string        */

typedef struct ASTNode
{
    /* What kind of construct this node represents */
    NodeType type;

    /* Optional textual payload:
       - identifier nodes  → variable or function name
       - literal nodes     → the literal text ("42", "3.14", "hello")
       - operator nodes    → operator symbol ("+", "==", "not", ...)
       - empty string for structural nodes (if, while, block, …)      */
    char value[AST_MAX_VALUE];

    /* Semantic type — STYPE_UNKNOWN until the semantic-analysis pass   */
    SemanticType inferred_type;

    /* Ordered child nodes (owned; free with ast_node_free)             */
    struct ASTNode **children;
    int             children_count;
    int             children_capacity;

    /* Source position of the first token of this construct             */
    int line;
    int column;
} ASTNode;

/* ------------------------------------------------------------------ */
/* API declared here, implemented in ast_ds.c                          */
/* ------------------------------------------------------------------ */

/* Allocate and initialise a new node; returns NULL on allocation failure. */
ASTNode *ast_node_create(NodeType type, const char *value, int line, int column);

/* Append a child to a node; returns 0 on success, -1 on allocation failure. */
int ast_node_add_child(ASTNode *parent, ASTNode *child);

/* Recursively free a node and all its descendants. */
void ast_node_free(ASTNode *node);

#endif

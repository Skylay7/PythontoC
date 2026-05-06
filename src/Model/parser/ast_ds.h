/* ast_ds.h */

#ifndef AST_DS_H
#define AST_DS_H

#include "node_types.h"
#include "../Symantic analayzer/semantic_types.h"

#define AST_MAX_VALUE 256

typedef struct ASTNode
{
    NodeType type;
    char value[AST_MAX_VALUE];  // identifier name, literal text, or operator symbol
    SemanticType inferred_type; // STYPE_UNKNOWN until semantic analysis runs

    struct ASTNode **children; // owned dynamic array, freed by ast_node_free
    int children_count;
    int children_capacity;

    int line;
    int column;
} ASTNode;

ASTNode *ast_node_create(NodeType type, const char *value, int line, int column);
int ast_node_add_child(ASTNode *parent, ASTNode *child);
void ast_node_free(ASTNode *node);

#endif

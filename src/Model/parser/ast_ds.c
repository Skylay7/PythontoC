/* ast_ds.c */

#include "ast_ds.h"
#include <stdlib.h>
#include <string.h>

#define CHILDREN_INITIAL_CAPACITY 4

ASTNode *ast_node_create(NodeType type, const char *value, int line, int column)
{
    ASTNode *node = malloc(sizeof(ASTNode));
    if (!node)
        return NULL;

    node->type = type;
    node->inferred_type = STYPE_UNKNOWN; // default until semantic analysis
    node->children = NULL;
    node->children_count = 0;
    node->children_capacity = 0;
    node->line = line;
    node->column = column;

    // copy value if provided, else set to empty string
    if (value)
    {
        strncpy(node->value, value, AST_MAX_VALUE - 1);
        node->value[AST_MAX_VALUE - 1] = '\0';
    }
    else
    {
        node->value[0] = '\0';
    }

    return node;
}

int ast_node_add_child(ASTNode *parent, ASTNode *child)
{
    if (parent->children_count >= parent->children_capacity)
    {
        // Need to grow the children array times 2 or start with initial capacity
        int new_cap = parent->children_capacity == 0
                          ? CHILDREN_INITIAL_CAPACITY
                          : parent->children_capacity * 2;

        ASTNode **new_children = realloc(parent->children,
                                         sizeof(ASTNode *) * (size_t)new_cap);
        if (!new_children)
            return -1;

        parent->children = new_children;
        parent->children_capacity = new_cap;
    }

    parent->children[parent->children_count++] = child;
    return 0;
}

void ast_node_free(ASTNode *node)
{
    if (!node)
        return;

    for (int i = 0; i < node->children_count; i++)
        ast_node_free(node->children[i]);

    free(node->children);
    free(node);
}

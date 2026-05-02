/* symantic_analyzer.c
 *
 * Implements:
 *   - Scope resolution  : linked chain of hash-table symbol tables,
 *                         one per block / function.
 *   - Type inference    : bottom-up, constraint-based.  Every node
 *                         receives an inferred_type from SemanticType.
 */

#include "symantic_analyzer.h"
#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/* Symbol table — open-address hash table, chained via parent pointer  */
/* ------------------------------------------------------------------ */

static unsigned int symbol_hash(const char *name)
{
    unsigned int h = 0;
    while (*name)
    {
        h = h * 31 + (unsigned char)*name;
        name++;
    }
    return h;
}

/* Insert or overwrite a name→type binding in the given scope. */
static void symbol_table_set(SymbolTable *table,
                             const char *name, SemanticType type)
{
    unsigned int index = symbol_hash(name) & (SYMBOL_TABLE_SIZE - 1);
    int probes = 0;
    while (table->entries[index].occupied &&
           strcmp(table->entries[index].name, name) != 0 &&
           probes < SYMBOL_TABLE_SIZE)
    {
        index = (index + 1) & (SYMBOL_TABLE_SIZE - 1);
        probes++;
    }
    strncpy(table->entries[index].name, name, MAX_SYMBOL_NAME - 1);
    table->entries[index].name[MAX_SYMBOL_NAME - 1] = '\0';
    table->entries[index].type = type;
    table->entries[index].occupied = 1;
}

/* Look up a name, walking outward through parent scopes.
   Returns STYPE_UNKNOWN if not found anywhere. */
static SemanticType symbol_table_lookup(const SymbolTable *table,
                                        const char *name)
{
    while (table)
    {
        unsigned int index = symbol_hash(name) & (SYMBOL_TABLE_SIZE - 1);
        int probes = 0;
        while (table->entries[index].occupied && probes < SYMBOL_TABLE_SIZE)
        {
            if (strcmp(table->entries[index].name, name) == 0)
                return table->entries[index].type;
            index = (index + 1) & (SYMBOL_TABLE_SIZE - 1);
            probes++;
        }
        table = table->parent; /* climb to enclosing scope */
    }
    return STYPE_UNKNOWN;
}

static void symbol_table_init(SymbolTable *table, SymbolTable *parent)
{
    for (int i = 0; i < SYMBOL_TABLE_SIZE; i++)
        table->entries[i].occupied = 0;
    table->parent = parent;
}

/* ------------------------------------------------------------------ */
/* Type inference helpers                                               */
/* ------------------------------------------------------------------ */

/* Promotion rule: int op float → float, anything else → error. */
static SemanticType promote(SemanticType a, SemanticType b)
{
    if (a == b)
        return a;
    if (a == STYPE_INT && b == STYPE_FLOAT)
        return STYPE_FLOAT;
    if (a == STYPE_FLOAT && b == STYPE_INT)
        return STYPE_FLOAT;
    return STYPE_ERROR;
}

/* Returns 1 if the type can appear in a condition (bool, int, float). */
static int is_condition_type(SemanticType t)
{
    return t == STYPE_BOOL || t == STYPE_INT || t == STYPE_FLOAT;
}

/* Forward declaration — infer_type and analyze_node are mutually recursive. */
static SemanticType infer_type(SemanticAnalyzer *sa, ASTNode *node);
static void analyze_node(SemanticAnalyzer *sa, ASTNode *node);

/* ------------------------------------------------------------------ */
/* Type inference — walks the expression subtree bottom-up             */
/* ------------------------------------------------------------------ */

static SemanticType infer_type(SemanticAnalyzer *sa, ASTNode *node)
{
    if (!node)
        return STYPE_UNKNOWN;

    SemanticType result;

    switch (node->type)
    {
    /* Leaves: type comes directly from syntax */
    case NODE_INT_LITERAL:
        result = STYPE_INT;
        break;

    case NODE_FLOAT_LITERAL:
        result = STYPE_FLOAT;
        break;

    case NODE_STRING_LITERAL:
        result = STYPE_STRING;
        break;

    case NODE_BOOL_LITERAL:
        result = STYPE_BOOL;
        break;

    case NODE_NONE_LITERAL:
        result = STYPE_NONE;
        break;

    /* Identifier: look up in symbol table chain */
    case NODE_IDENTIFIER:
        result = symbol_table_lookup(sa->current, node->value);
        if (result == STYPE_UNKNOWN)
            error_list_add(sa->errors,
                           "Use of undeclared variable",
                           node->line, node->column);
        break;

    /* Arithmetic / comparison binary operators */
    case NODE_BINARY_OP:
    {
        SemanticType left = infer_type(sa, node->children_count > 0 ? node->children[0] : NULL);
        SemanticType right = infer_type(sa, node->children_count > 1 ? node->children[1] : NULL);

        /* Comparison operators always yield bool */
        const char *op = node->value;
        if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0 ||
            strcmp(op, "<") == 0 || strcmp(op, ">") == 0 ||
            strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0 ||
            strcmp(op, "and") == 0 || strcmp(op, "or") == 0 ||
            strcmp(op, "in") == 0)
        {
            result = STYPE_BOOL;
        }
        else
        {
            result = promote(left, right);
            if (result == STYPE_ERROR)
                error_list_add(sa->errors,
                               "Type mismatch in binary expression",
                               node->line, node->column);
        }
        break;
    }

    /* Unary operators */
    case NODE_UNARY_OP:
    {
        SemanticType operand = infer_type(sa, node->children_count > 0 ? node->children[0] : NULL);
        if (strcmp(node->value, "not") == 0)
            result = STYPE_BOOL;
        else
            result = operand; /* unary minus keeps the operand type */
        break;
    }

    /* Function call: look up return type in symbol table */
    case NODE_FUNCTION_CALL:
        result = symbol_table_lookup(sa->current, node->value);
        if (result == STYPE_UNKNOWN)
        {
            /* Infer arguments for side-effect type checking */
            for (int i = 0; i < node->children_count; i++)
                infer_type(sa, node->children[i]);
            /* Return type unknown until function is defined */
            result = STYPE_UNKNOWN;
        }
        break;

    default:
        result = STYPE_UNKNOWN;
        break;
    }

    node->inferred_type = result;
    return result;
}

/* ------------------------------------------------------------------ */
/* Node analysis — handles statements; calls infer_type for exprs      */
/* ------------------------------------------------------------------ */

/* Push a new child scope onto the chain and return it (heap-allocated). */
static SymbolTable *push_scope(SemanticAnalyzer *sa)
{
    SymbolTable *child = malloc(sizeof(SymbolTable));
    if (!child)
        return sa->current; /* fallback: stay in current scope */
    symbol_table_init(child, sa->current);
    sa->current = child;
    return child;
}

/* Pop back to parent scope and free the child. */
static void pop_scope(SemanticAnalyzer *sa)
{
    SymbolTable *child = sa->current;
    if (child && child->parent)
    {
        sa->current = child->parent;
        free(child);
    }
}

static void analyze_node(SemanticAnalyzer *sa, ASTNode *node)
{
    if (!node)
        return;

    switch (node->type)
    {
    /* 5.1 — skip error nodes */
    case NODE_IDENTIFIER:
        if (strcmp(node->value, "<error>") == 0)
            return;
        break;

    /* 5.2 — assignment: infer RHS type, bind variable in current scope */
    case NODE_ASSIGN:
    case NODE_ASSIGN_PLUS:
    case NODE_ASSIGN_MINUS:
    case NODE_ASSIGN_MULT:
    case NODE_ASSIGN_DIV:
    {
        ASTNode *target = node->children_count > 0 ? node->children[0] : NULL;
        ASTNode *value = node->children_count > 1 ? node->children[1] : NULL;

        SemanticType vtype = infer_type(sa, value);

        if (vtype == STYPE_ERROR)
            error_list_add(sa->errors,
                           "Type mismatch in assignment expression",
                           node->line, node->column);

        if (target && target->type == NODE_IDENTIFIER)
        {
            symbol_table_set(sa->current, target->value, vtype);
            target->inferred_type = vtype;
        }

        node->inferred_type = vtype;
        return;
    }

    /* 5.3 — if / elif / while: check condition type, open new scope for body */
    case NODE_IF:
    case NODE_ELIF:
    case NODE_WHILE:
    {
        /* child[0] = condition expression */
        if (node->children_count > 0)
        {
            SemanticType ctype = infer_type(sa, node->children[0]);
            if (!is_condition_type(ctype))
                error_list_add(sa->errors,
                               "Non-logical type in condition expression",
                               node->line, node->column);
        }

        /* Remaining children are BLOCK / ELIF / ELSE nodes — open scope each */
        for (int i = 1; i < node->children_count; i++)
        {
            push_scope(sa);
            analyze_node(sa, node->children[i]);
            pop_scope(sa);
        }
        return;
    }

    case NODE_ELSE:
    {
        /* No condition — just a block */
        push_scope(sa);
        for (int i = 0; i < node->children_count; i++)
            analyze_node(sa, node->children[i]);
        pop_scope(sa);
        return;
    }

    /* FOR loop: loop variable is int (range index), open scope for body */
    case NODE_FOR:
    {
        /* child[0] = loop variable, child[1] = NODE_RANGE, child[2] = BLOCK */
        if (node->children_count > 0 && node->children[0])
        {
            symbol_table_set(sa->current,
                             node->children[0]->value, STYPE_INT);
            node->children[0]->inferred_type = STYPE_INT;
        }
        if (node->children_count > 1)
            infer_type(sa, node->children[1]); /* validate range expr */

        push_scope(sa);
        if (node->children_count > 2)
            analyze_node(sa, node->children[2]);
        pop_scope(sa);
        return;
    }

    /* 5.4 — print: infer argument types (used by code generator for format) */
    case NODE_PRINT:
        for (int i = 0; i < node->children_count; i++)
        {
            SemanticType at = infer_type(sa, node->children[i]);
            node->children[i]->inferred_type = at;
        }
        return;

    /* 5.5 — function definition: register name, open scope, add params */
    case NODE_DEF:
    {
        /* Register the function name in the parent (current) scope */
        symbol_table_set(sa->current, node->value, STYPE_FUNCTION);

        push_scope(sa);

        /* child[0] = NODE_PARAM_LIST, child[1] = NODE_BLOCK */
        if (node->children_count > 0 &&
            node->children[0]->type == NODE_PARAM_LIST)
        {
            ASTNode *params = node->children[0];
            for (int i = 0; i < params->children_count; i++)
            {
                ASTNode *p = params->children[i];
                if (p && p->type == NODE_IDENTIFIER)
                    symbol_table_set(sa->current, p->value, STYPE_UNKNOWN);
            }
        }

        if (node->children_count > 1)
            analyze_node(sa, node->children[1]);

        pop_scope(sa);
        return;
    }

    /* RETURN: infer the return expression type */
    case NODE_RETURN:
        if (node->children_count > 0)
        {
            SemanticType rt = infer_type(sa, node->children[0]);
            node->inferred_type = rt;
        }
        return;

    /* BLOCK: analyze each statement in order */
    case NODE_BLOCK:
        for (int i = 0; i < node->children_count; i++)
            analyze_node(sa, node->children[i]);
        return;

    /* BREAK / CONTINUE / PASS: nothing to infer */
    case NODE_BREAK:
    case NODE_CONTINUE:
    case NODE_PASS:
        return;

    default:
        /* Expression nodes not handled above — run type inference */
        infer_type(sa, node);
        return;
    }

    /* Default fall-through: recurse into children */
    for (int i = 0; i < node->children_count; i++)
        analyze_node(sa, node->children[i]);
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

ASTNode *semantic_analyze(ASTNode *root, ErrorList *errors)
{
    if (!root)
        return NULL;

    SemanticAnalyzer sa;
    sa.errors = errors;
    symbol_table_init(&sa.global, NULL);
    sa.current = &sa.global;

    /* Walk from the root (NODE_PROGRAM) downward */
    if (root->type == NODE_PROGRAM)
    {
        for (int i = 0; i < root->children_count; i++)
            analyze_node(&sa, root->children[i]);
    }
    else
    {
        analyze_node(&sa, root);
    }

    return root;
}

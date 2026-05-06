/* symantic_analyzer.c */

#include "symantic_analyzer.h"
#include <string.h>
#include <stdlib.h>

// Symbol table — open-address hash table with linear probing.
// Each scope is a separate SymbolTable linked to its parent via the parent pointer.
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

static void symbol_table_set(SymbolTable *table, const char *name, SemanticType type)
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

/* Walks up the scope chain. Returns 1 if found (type written to *out_type), 0 if not declared. */
static int symbol_table_lookup(const SymbolTable *table, const char *name, SemanticType *out_type)
{
    while (table)
    {
        unsigned int index = symbol_hash(name) & (SYMBOL_TABLE_SIZE - 1);
        int probes = 0;
        while (table->entries[index].occupied && probes < SYMBOL_TABLE_SIZE)
        {
            if (strcmp(table->entries[index].name, name) == 0)
            {
                *out_type = table->entries[index].type;
                return 1;
            }
            index = (index + 1) & (SYMBOL_TABLE_SIZE - 1);
            probes++;
        }
        table = table->parent;
    }
    return 0;
}

static void symbol_table_init(SymbolTable *table, SymbolTable *parent)
{
    for (int i = 0; i < SYMBOL_TABLE_SIZE; i++)
        table->entries[i].occupied = 0;
    table->parent = parent;
}

/* int+float → float. UNKNOWN on either side means we don't know yet, so trust the other side. */
static SemanticType promote(SemanticType a, SemanticType b)
{
    if (a == b)
        return a;
    if (a == STYPE_UNKNOWN)
        return b;
    if (b == STYPE_UNKNOWN)
        return a;
    if (a == STYPE_INT && b == STYPE_FLOAT)
        return STYPE_FLOAT;
    if (a == STYPE_FLOAT && b == STYPE_INT)
        return STYPE_FLOAT;
    return STYPE_ERROR;
}

/* Merges two observed return types into one. STYPE_NONE is the neutral element (no return seen yet).
   Returns STYPE_UNKNOWN if the types conflict with no valid promotion. */
static SemanticType promote_return(SemanticType a, SemanticType b)
{
    if (a == b)
        return a;
    if (a == STYPE_NONE)
        return b;
    if (b == STYPE_NONE)
        return a;
    if (a == STYPE_INT && b == STYPE_FLOAT)
        return STYPE_FLOAT;
    if (a == STYPE_FLOAT && b == STYPE_INT)
        return STYPE_FLOAT;
    return STYPE_UNKNOWN; // conflict
}

static int is_condition_type(SemanticType t)
{
    return t == STYPE_BOOL || t == STYPE_INT || t == STYPE_FLOAT;
}

/* Walks the scope chain to find a variable and refines its type if we learn something
   stronger from context. Only promotes forward (int→float, int→string); never downgrades.
   First refinement wins — once a type is set to something other than STYPE_INT, it stays. */
static void refine_identifier_type(SemanticAnalyzer *sa, const ASTNode *node, SemanticType new_type)
{
    if (!node || node->type != NODE_IDENTIFIER)
        return;
    if (new_type == STYPE_UNKNOWN || new_type == STYPE_ERROR || new_type == STYPE_INT)
        return;

    SymbolTable *table = sa->current;
    while (table)
    {
        unsigned int index = symbol_hash(node->value) & (SYMBOL_TABLE_SIZE - 1);
        int probes = 0;
        while (table->entries[index].occupied && probes < SYMBOL_TABLE_SIZE)
        {
            if (strcmp(table->entries[index].name, node->value) == 0)
            {
                // only refine if still at the default (STYPE_INT); first evidence wins
                if (table->entries[index].type == STYPE_INT)
                    table->entries[index].type = new_type;
                return;
            }
            index = (index + 1) & (SYMBOL_TABLE_SIZE - 1);
            probes++;
        }
        table = table->parent;
    }
}

static SemanticType infer_type(SemanticAnalyzer *sa, ASTNode *node);
static void analyze_node(SemanticAnalyzer *sa, ASTNode *node);

/* Bottom-up type inference for expression nodes.
   Walks the subtree, sets node->inferred_type on each node, and returns the type.
   Comparison operators always yield STYPE_BOOL regardless of operand types. */
static SemanticType infer_type(SemanticAnalyzer *sa, ASTNode *node)
{
    if (!node)
        return STYPE_UNKNOWN;

    SemanticType result;

    switch (node->type)
    {
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

    case NODE_IDENTIFIER:
    {
        SemanticType found_type = STYPE_UNKNOWN;
        if (!symbol_table_lookup(sa->current, node->value, &found_type))
            error_list_add_staged(sa->errors, "Use of undeclared variable",
                                  node->line, node->column, STAGE_SEMANTIC);
        result = found_type;
        break;
    }

    case NODE_BINARY_OP:
    {
        SemanticType left = infer_type(sa, node->children_count > 0 ? node->children[0] : NULL);
        SemanticType right = infer_type(sa, node->children_count > 1 ? node->children[1] : NULL);

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
            ASTNode *left_node = node->children_count > 0 ? node->children[0] : NULL;
            ASTNode *right_node = node->children_count > 1 ? node->children[1] : NULL;

            // string + identifier: refine the identifier to string before promote
            if (strcmp(op, "+") == 0)
            {
                if (left == STYPE_STRING)
                    refine_identifier_type(sa, right_node, STYPE_STRING);
                if (right == STYPE_STRING)
                    refine_identifier_type(sa, left_node, STYPE_STRING);
            }

            // read back updated types from the symbol table without re-running infer_type
            // (re-running would emit duplicate "undeclared variable" errors)
            if (left_node && left_node->type == NODE_IDENTIFIER)
            {
                SemanticType updated = left;
                symbol_table_lookup(sa->current, left_node->value, &updated);
                left = updated;
            }
            if (right_node && right_node->type == NODE_IDENTIFIER)
            {
                SemanticType updated = right;
                symbol_table_lookup(sa->current, right_node->value, &updated);
                right = updated;
            }

            result = promote(left, right);
            if (result == STYPE_ERROR)
            {
                error_list_add_staged(sa->errors, "Type mismatch in binary expression",
                                      node->line, node->column, STAGE_SEMANTIC);
            }
            else
            {
                // if arithmetic promotion changed one side, refine that identifier too
                if (result != left)
                    refine_identifier_type(sa, left_node, result);
                if (result != right)
                    refine_identifier_type(sa, right_node, result);
            }
        }
        break;
    }

    case NODE_UNARY_OP:
    {
        SemanticType operand = infer_type(sa, node->children_count > 0 ? node->children[0] : NULL);
        result = strcmp(node->value, "not") == 0 ? STYPE_BOOL : operand;
        break;
    }

    case NODE_FUNCTION_CALL:
    {
        SemanticType fn_type = STYPE_UNKNOWN;
        symbol_table_lookup(sa->current, node->value, &fn_type);
        for (int i = 0; i < node->children_count; i++)
            infer_type(sa, node->children[i]);
        result = fn_type;
        break;
    }

    default:
        result = STYPE_UNKNOWN;
        break;
    }

    node->inferred_type = result;
    return result;
}

// Scope management — each block/function gets its own heap-allocated scope.
static SymbolTable *push_scope(SemanticAnalyzer *sa)
{
    SymbolTable *child = malloc(sizeof(SymbolTable));
    if (!child)
        return sa->current;
    symbol_table_init(child, sa->current);
    sa->current = child;
    return child;
}

static void pop_scope(SemanticAnalyzer *sa)
{
    SymbolTable *child = sa->current;
    if (child && child->parent)
    {
        sa->current = child->parent;
        free(child);
    }
}

/* Handles statement-level nodes — binds variables, opens/closes scopes, checks conditions.
   Calls infer_type for expression subtrees rather than recursing into them directly. */
static void analyze_node(SemanticAnalyzer *sa, ASTNode *node)
{
    if (!node)
        return;

    switch (node->type)
    {
    case NODE_IDENTIFIER:
        if (strcmp(node->value, "<error>") == 0)
            return;
        break;

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
            error_list_add_staged(sa->errors, "Type mismatch in assignment expression",
                                  node->line, node->column, STAGE_SEMANTIC);

        if (target && target->type == NODE_IDENTIFIER)
        {
            symbol_table_set(sa->current, target->value, vtype);
            target->inferred_type = vtype;
        }

        node->inferred_type = vtype;
        return;
    }

    case NODE_IF:
    case NODE_ELIF:
    case NODE_WHILE:
    {
        if (node->children_count > 0)
        {
            SemanticType ctype = infer_type(sa, node->children[0]);
            if (!is_condition_type(ctype))
                error_list_add_staged(sa->errors, "Non-logical type in condition expression",
                                      node->line, node->column, STAGE_SEMANTIC);
        }
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
        push_scope(sa);
        for (int i = 0; i < node->children_count; i++)
            analyze_node(sa, node->children[i]);
        pop_scope(sa);
        return;
    }

    case NODE_FOR:
    {
        // loop variable is always int (range index)
        if (node->children_count > 0 && node->children[0])
        {
            symbol_table_set(sa->current, node->children[0]->value, STYPE_INT);
            node->children[0]->inferred_type = STYPE_INT;
        }
        if (node->children_count > 1)
            infer_type(sa, node->children[1]);

        push_scope(sa);
        if (node->children_count > 2)
            analyze_node(sa, node->children[2]);
        pop_scope(sa);
        return;
    }

    case NODE_PRINT:
        for (int i = 0; i < node->children_count; i++)
        {
            SemanticType at = infer_type(sa, node->children[i]);
            node->children[i]->inferred_type = at;
        }
        return;

    case NODE_DEF:
    {
        // register as STYPE_UNKNOWN initially; updated to actual return type after body analysis
        symbol_table_set(sa->current, node->value, STYPE_UNKNOWN);
        push_scope(sa);

        SemanticType saved_return = sa->current_fn_return;
        sa->current_fn_return = STYPE_NONE;

        ASTNode *params = (node->children_count > 0 &&
                           node->children[0]->type == NODE_PARAM_LIST)
                              ? node->children[0]
                              : NULL;

        // default all params to int — refine_identifier_type will update if body proves otherwise
        if (params)
        {
            for (int i = 0; i < params->children_count; i++)
            {
                ASTNode *p = params->children[i];
                if (p && p->type == NODE_IDENTIFIER)
                {
                    symbol_table_set(sa->current, p->value, STYPE_INT);
                    p->inferred_type = STYPE_INT;
                }
            }
        }

        if (node->children_count > 1)
            analyze_node(sa, node->children[1]);

        // read back refined param types after body analysis
        if (params)
        {
            for (int i = 0; i < params->children_count; i++)
            {
                ASTNode *p = params->children[i];
                if (p && p->type == NODE_IDENTIFIER)
                {
                    SemanticType refined = STYPE_INT;
                    symbol_table_lookup(sa->current, p->value, &refined);
                    p->inferred_type = refined;
                }
            }
        }

        SemanticType final_ret = sa->current_fn_return; // STYPE_NONE if no return statement
        node->inferred_type = final_ret;
        sa->current_fn_return = saved_return;

        pop_scope(sa);
        // update function's type in parent scope with the inferred return type
        symbol_table_set(sa->current, node->value, final_ret);
        return;
    }

    case NODE_RETURN:
        if (node->children_count > 0)
        {
            SemanticType rt = infer_type(sa, node->children[0]);
            node->inferred_type = rt;
            sa->current_fn_return = promote_return(sa->current_fn_return, rt);
            if (sa->current_fn_return == STYPE_UNKNOWN)
                error_list_add_staged(sa->errors, "Function has inconsistent return types",
                                      node->line, node->column, STAGE_SEMANTIC);
        }
        return;

    case NODE_BLOCK:
        for (int i = 0; i < node->children_count; i++)
            analyze_node(sa, node->children[i]);
        return;

    case NODE_BREAK:
    case NODE_CONTINUE:
    case NODE_PASS:
        return;
    // other statement types (NODE_PROGRAM, NODE_ARG_LIST, NODE_FUNCTION_CALL etc.) don't need special handling here
    default:
        infer_type(sa, node);
        return;
    }

    for (int i = 0; i < node->children_count; i++)
        analyze_node(sa, node->children[i]);
}

/* Entry point. Initializes the global scope and walks the program top-down.
   Returns the same root pointer with inferred_type filled in on every node. */
ASTNode *semantic_analyze(ASTNode *root, ErrorList *errors)
{
    if (!root)
        return NULL;

    SemanticAnalyzer sa;
    sa.errors = errors;
    sa.current_fn_return = STYPE_NONE;
    symbol_table_init(&sa.global, NULL);
    sa.current = &sa.global;

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

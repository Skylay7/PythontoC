/* code_generator.c
 *
 * Walks the typed AST and produces C source text. Function definitions
 * go into a separate buffer so they appear before main() in the output.
 * Variable declarations are tracked via the declared_in_c flag on the
 * symbol table entries produced by the semantic analyzer — no separate
 * DeclTable needed.
 */

#include "code_generator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Dynamic output buffer — grows by doubling when full, similar to a StringBuilder.
typedef struct
{
    char *data;
    int   len;
    int   cap;
} CodeBuf;

static int buf_init(CodeBuf *b)
{
    b->cap  = 4096;
    b->len  = 0;
    b->data = malloc((size_t)b->cap);
    if (b->data)
        b->data[0] = '\0';
    return b->data ? 0 : -1;
}

static void buf_free(CodeBuf *b)
{
    free(b->data);
    b->data = NULL;
    b->len = b->cap = 0;
}

static int buf_write(CodeBuf *b, const char *s)
{
    int slen = (int)strlen(s);
    if (b->len + slen + 1 > b->cap)
    {
        int   new_cap  = b->cap * 2 + slen + 1;
        char *new_data = realloc(b->data, (size_t)new_cap);
        if (!new_data)
            return -1;
        b->data = new_data;
        b->cap  = new_cap;
    }
    memcpy(b->data + b->len, s, (size_t)slen);
    b->len += slen;
    b->data[b->len] = '\0';
    return 0;
}

static const char *stype_to_c(SemanticType t)
{
    switch (t)
    {
    case STYPE_FLOAT:  return "double";
    case STYPE_STRING: return "const char *";
    case STYPE_BOOL:   return "int";
    case STYPE_NONE:   return "void";
    default:           return "int";
    }
}

static const char *stype_to_fmt(SemanticType t)
{
    switch (t)
    {
    case STYPE_FLOAT:  return "%f";
    case STYPE_STRING: return "%s";
    default:           return "%d";
    }
}

// C operators for compound assignments, indexed by (NodeType - NODE_ASSIGN_PLUS).
static const char *compound_op[] = {"+=", "-=", "*=", "/="};

typedef struct
{
    CodeBuf     *buf;
    SymbolTable *current_scope; // mirrors the semantic analyzer's scope tree
    int          indent;
    int          tmp_counter;   // for generating unique __tmp_N temp variable names
    ErrorList   *errors;
} CodeGen;

/* Advance into the next child scope of the current scope. */
static void cg_scope_enter(CodeGen *cg)
{
    SymbolTable *s = cg->current_scope;
    if (s && s->next_child < s->child_count)
        cg->current_scope = s->children[s->next_child++];
}

/* Step back to the parent scope. */
static void cg_scope_exit(CodeGen *cg)
{
    if (cg->current_scope && cg->current_scope->parent)
        cg->current_scope = cg->current_scope->parent;
}

/* Resolves the type of a node.
   Identifiers and function calls are looked up in the scope chain — symbol table is the authority.
   Everything else (literals, operators) reads node->inferred_type since those have no table entry. */
static SemanticType get_type(CodeGen *cg, const ASTNode *node)
{
    if (!node)
        return STYPE_UNKNOWN;
    if (node->type == NODE_IDENTIFIER || node->type == NODE_FUNCTION_CALL)
    {
        SymbolTable *scope = cg->current_scope;
        while (scope)
        {
            SymbolEntry *e = symbol_table_lookup_local(scope, node->value);
            if (e)
                return e->type;
            scope = scope->parent;
        }
        return STYPE_UNKNOWN;
    }
    return node->inferred_type;
}

static void write_indent(CodeGen *cg)
{
    for (int i = 0; i < cg->indent; i++)
        buf_write(cg->buf, "    ");
}

static void gen_expr(CodeGen *cg, const ASTNode *node);
static void gen_stmt(CodeGen *cg, const ASTNode *node);

// Returns 1 if node is a string + string concatenation.
static int is_str_concat(const ASTNode *node)
{
    return node &&
           node->type == NODE_BINARY_OP &&
           strcmp(node->value, "+") == 0 &&
           node->inferred_type == STYPE_STRING;
}

/* Emits  char __tmp_N[256];  and  snprintf(__tmp_N, 256, "%s%s", left, right);
   and writes the temp name into out_name. Used before statements that contain string concats. */
static void gen_str_concat_temp(CodeGen *cg, const ASTNode *node, char *out_name, int out_size)
{
    snprintf(out_name, out_size, "__tmp_%d", cg->tmp_counter++);
    write_indent(cg);
    buf_write(cg->buf, "char ");
    buf_write(cg->buf, out_name);
    buf_write(cg->buf, "[256];\n");
    write_indent(cg);
    buf_write(cg->buf, "snprintf(");
    buf_write(cg->buf, out_name);
    buf_write(cg->buf, ", 256, \"%s%s\", ");
    gen_expr(cg, node->children_count > 0 ? node->children[0] : NULL);
    buf_write(cg->buf, ", ");
    gen_expr(cg, node->children_count > 1 ? node->children[1] : NULL);
    buf_write(cg->buf, ");\n");
}

/* Pre-generates a temp buffer for each string-concat arg in children[0..count-1].
   tmp_names[i] is set to the temp name if child i needed one, or left empty otherwise.
   Returns the number of children processed. */
static void gen_concat_temps(CodeGen *cg, ASTNode **children, int count,
                              char tmp_names[][32], int tmp_name_size)
{
    for (int i = 0; i < count && i < tmp_name_size; i++)
    {
        if (is_str_concat(children[i]))
            gen_str_concat_temp(cg, children[i], tmp_names[i], 32);
        else
            tmp_names[i][0] = '\0';
    }
}

/* Emits a comma-separated argument list using pre-generated temps where available. */
static void gen_args(CodeGen *cg, ASTNode **children, int count,
                     char tmp_names[][32], int tmp_name_size)
{
    for (int i = 0; i < count && i < tmp_name_size; i++)
    {
        if (i > 0)
            buf_write(cg->buf, ", ");
        if (tmp_names[i][0] != '\0')
            buf_write(cg->buf, tmp_names[i]);
        else
            gen_expr(cg, children[i]);
    }
}

/* Writes an expression inline to the buffer — no indentation, no newline.
   Called recursively for sub-expressions (binary ops, function call args, etc.). */
static void gen_expr(CodeGen *cg, const ASTNode *node)
{
    if (!node)
        return;

    switch (node->type)
    {
    case NODE_INT_LITERAL:
    case NODE_FLOAT_LITERAL:
        buf_write(cg->buf, node->value);
        break;

    case NODE_STRING_LITERAL:
        buf_write(cg->buf, "\"");
        buf_write(cg->buf, node->value);
        buf_write(cg->buf, "\"");
        break;

    case NODE_BOOL_LITERAL:
        buf_write(cg->buf, strcmp(node->value, "True") == 0 ? "1" : "0");
        break;

    case NODE_NONE_LITERAL:
        buf_write(cg->buf, "0");
        break;

    case NODE_IDENTIFIER:
        buf_write(cg->buf, node->value);
        break;

    case NODE_UNARY_OP:
    {
        const char *c_op = strcmp(node->value, "not") == 0 ? "!" : node->value;
        buf_write(cg->buf, c_op);
        buf_write(cg->buf, "(");
        gen_expr(cg, node->children_count > 0 ? node->children[0] : NULL);
        buf_write(cg->buf, ")");
        break;
    }

    case NODE_BINARY_OP:
    {
        const char *op   = node->value;
        const char *c_op = op; // most Python operators map directly to C

        if (strcmp(op, "and") == 0)       c_op = "&&";
        else if (strcmp(op, "or") == 0)   c_op = "||";
        else if (strcmp(op, "//") == 0)   c_op = "/";

        if (strcmp(op, "**") == 0)
        {
            // no ** in C, use pow()
            buf_write(cg->buf, "pow(");
            gen_expr(cg, node->children_count > 0 ? node->children[0] : NULL);
            buf_write(cg->buf, ", ");
            gen_expr(cg, node->children_count > 1 ? node->children[1] : NULL);
            buf_write(cg->buf, ")");
        }
        else
        {
            buf_write(cg->buf, "(");
            gen_expr(cg, node->children_count > 0 ? node->children[0] : NULL);
            buf_write(cg->buf, " ");
            buf_write(cg->buf, c_op);
            buf_write(cg->buf, " ");
            gen_expr(cg, node->children_count > 1 ? node->children[1] : NULL);
            buf_write(cg->buf, ")");
        }
        break;
    }

    case NODE_FUNCTION_CALL:
    {
        buf_write(cg->buf, node->value);
        buf_write(cg->buf, "(");
        for (int i = 0; i < node->children_count; i++)
        {
            if (i > 0)
                buf_write(cg->buf, ", ");
            gen_expr(cg, node->children[i]);
        }
        buf_write(cg->buf, ")");
        break;
    }

    case NODE_RANGE:
        // range(n) only stores the upper bound — emit it directly
        gen_expr(cg, node->children_count > 0 ? node->children[0] : NULL);
        break;

    default:
        // fallback: emit the raw value token if present (e.g. unknown operator)
        if (node->value[0] != '\0')
            buf_write(cg->buf, node->value);
        break;
    }
}

/* Writes a full statement to the buffer, including indentation and trailing newline.
   Handles all statement node types; delegates expressions to gen_expr. */
static void gen_stmt(CodeGen *cg, const ASTNode *node)
{
    if (!node)
        return;

    switch (node->type)
    {
    case NODE_BLOCK:
        for (int i = 0; i < node->children_count; i++)
            gen_stmt(cg, node->children[i]);
        break;

    case NODE_ASSIGN:
    {
        const ASTNode *target = node->children_count > 0 ? node->children[0] : NULL;
        const ASTNode *value  = node->children_count > 1 ? node->children[1] : NULL;
        if (!target)
            break;

        SymbolEntry *entry          = symbol_table_lookup_local(cg->current_scope, target->value);
        int          already_declared = entry && entry->declared_in_c;

        if (is_str_concat(value))
        {
            // string concat: declare as char[256] then snprintf into it
            if (!already_declared)
            {
                write_indent(cg);
                buf_write(cg->buf, "char ");
                buf_write(cg->buf, target->value);
                buf_write(cg->buf, "[256];\n");
                if (entry)
                    entry->declared_in_c = 1;
            }
            write_indent(cg);
            buf_write(cg->buf, "snprintf(");
            buf_write(cg->buf, target->value);
            buf_write(cg->buf, ", 256, \"%s%s\", ");
            gen_expr(cg, value->children_count > 0 ? value->children[0] : NULL);
            buf_write(cg->buf, ", ");
            gen_expr(cg, value->children_count > 1 ? value->children[1] : NULL);
            buf_write(cg->buf, ");\n");
        }
        else
        {
            write_indent(cg);
            if (!already_declared)
            {
                // prefer the symbol table's type; fall back to the node's inferred type
                SemanticType t = (entry && entry->type != STYPE_UNKNOWN && entry->type != STYPE_NONE)
                                     ? entry->type
                                 : (node->inferred_type != STYPE_UNKNOWN && node->inferred_type != STYPE_NONE)
                                     ? node->inferred_type
                                     : STYPE_INT;
                buf_write(cg->buf, stype_to_c(t));
                buf_write(cg->buf, " ");
                buf_write(cg->buf, target->value);
                buf_write(cg->buf, " = ");
                if (entry)
                    entry->declared_in_c = 1;
            }
            else
            {
                buf_write(cg->buf, target->value);
                buf_write(cg->buf, " = ");
            }
            gen_expr(cg, value);
            buf_write(cg->buf, ";\n");
        }
        break;
    }

    case NODE_ASSIGN_PLUS:
    case NODE_ASSIGN_MINUS:
    case NODE_ASSIGN_MULT:
    case NODE_ASSIGN_DIV:
    {
        const ASTNode *target = node->children_count > 0 ? node->children[0] : NULL;
        const ASTNode *value  = node->children_count > 1 ? node->children[1] : NULL;
        if (!target)
            break;

        write_indent(cg);
        buf_write(cg->buf, target->value);
        buf_write(cg->buf, " ");
        buf_write(cg->buf, compound_op[node->type - NODE_ASSIGN_PLUS]);
        buf_write(cg->buf, " ");
        gen_expr(cg, value);
        buf_write(cg->buf, ";\n");
        break;
    }

    case NODE_IF:
    {
        write_indent(cg);
        buf_write(cg->buf, "if (");
        gen_expr(cg, node->children_count > 0 ? node->children[0] : NULL);
        buf_write(cg->buf, ") {\n");
        cg->indent++;
        cg_scope_enter(cg);
        if (node->children_count > 1)
            gen_stmt(cg, node->children[1]);
        cg_scope_exit(cg);
        cg->indent--;
        write_indent(cg);
        buf_write(cg->buf, "}");
        // elif/else siblings are extra children; each gets the wrapper scope the IF pushed for it
        for (int i = 2; i < node->children_count; i++)
        {
            cg_scope_enter(cg);
            gen_stmt(cg, node->children[i]);
            cg_scope_exit(cg);
        }
        buf_write(cg->buf, "\n");
        break;
    }

    case NODE_ELIF:
    {
        buf_write(cg->buf, " else if (");
        gen_expr(cg, node->children_count > 0 ? node->children[0] : NULL);
        buf_write(cg->buf, ") {\n");
        cg->indent++;
        cg_scope_enter(cg);
        if (node->children_count > 1)
            gen_stmt(cg, node->children[1]);
        cg_scope_exit(cg);
        cg->indent--;
        write_indent(cg);
        buf_write(cg->buf, "}");
        for (int i = 2; i < node->children_count; i++)
        {
            cg_scope_enter(cg);
            gen_stmt(cg, node->children[i]);
            cg_scope_exit(cg);
        }
        break;
    }

    case NODE_ELSE:
    {
        buf_write(cg->buf, " else {\n");
        cg->indent++;
        cg_scope_enter(cg);
        for (int i = 0; i < node->children_count; i++)
            gen_stmt(cg, node->children[i]);
        cg_scope_exit(cg);
        cg->indent--;
        write_indent(cg);
        buf_write(cg->buf, "}");
        break;
    }

    case NODE_WHILE:
    {
        write_indent(cg);
        buf_write(cg->buf, "while (");
        gen_expr(cg, node->children_count > 0 ? node->children[0] : NULL);
        buf_write(cg->buf, ") {\n");
        cg->indent++;
        cg_scope_enter(cg);
        if (node->children_count > 1)
            gen_stmt(cg, node->children[1]);
        cg_scope_exit(cg);
        cg->indent--;
        write_indent(cg);
        buf_write(cg->buf, "}\n");
        break;
    }

    case NODE_FOR:
    {
        // child[0]=loop var, child[1]=NODE_RANGE, child[2]=body
        const ASTNode *var   = node->children_count > 0 ? node->children[0] : NULL;
        const ASTNode *range = node->children_count > 1 ? node->children[1] : NULL;
        const ASTNode *body  = node->children_count > 2 ? node->children[2] : NULL;
        const char    *vname = var ? var->value : "_i";

        write_indent(cg);
        buf_write(cg->buf, "for (int ");
        buf_write(cg->buf, vname);
        buf_write(cg->buf, " = 0; ");
        buf_write(cg->buf, vname);
        buf_write(cg->buf, " < ");
        gen_expr(cg, range);
        buf_write(cg->buf, "; ");
        buf_write(cg->buf, vname);
        buf_write(cg->buf, "++) {\n");
        cg->indent++;
        cg_scope_enter(cg);
        gen_stmt(cg, body);
        cg_scope_exit(cg);
        cg->indent--;
        write_indent(cg);
        buf_write(cg->buf, "}\n");
        break;
    }

    case NODE_PRINT:
    {
        if (node->children_count == 0)
        {
            write_indent(cg);
            buf_write(cg->buf, "printf(\"\\n\");\n");
            break;
        }
        char tmp_names[64][32];
        gen_concat_temps(cg, node->children, node->children_count, tmp_names, 64);
        write_indent(cg);
        buf_write(cg->buf, "printf(\"");
        for (int i = 0; i < node->children_count; i++)
        {
            if (i > 0)
                buf_write(cg->buf, " ");
            buf_write(cg->buf, stype_to_fmt(get_type(cg, node->children[i])));
        }
        buf_write(cg->buf, "\\n\"");
        buf_write(cg->buf, ", ");
        gen_args(cg, node->children, node->children_count, tmp_names, 64);
        buf_write(cg->buf, ");\n");
        break;
    }

    case NODE_DEF:
    {
        // child[0]=param list, child[1]=body block
        // return type lives in the current (outer) scope under the function's name
        SymbolEntry *fn_entry = symbol_table_lookup_local(cg->current_scope, node->value);
        SemanticType  ret     = fn_entry ? fn_entry->type : STYPE_INT;
        buf_write(cg->buf, stype_to_c(ret != STYPE_UNKNOWN ? ret : STYPE_INT));
        buf_write(cg->buf, " ");
        buf_write(cg->buf, node->value);
        buf_write(cg->buf, "(");

        // enter function scope now so param lookups hit the right scope
        cg_scope_enter(cg);

        if (node->children_count > 0 && node->children[0]->type == NODE_PARAM_LIST)
        {
            const ASTNode *params = node->children[0];
            for (int i = 0; i < params->children_count; i++)
            {
                if (i > 0)
                    buf_write(cg->buf, ", ");
                SymbolEntry *pe = symbol_table_lookup_local(cg->current_scope, params->children[i]->value);
                SemanticType pt = (pe && pe->type != STYPE_UNKNOWN) ? pe->type : STYPE_INT;
                buf_write(cg->buf, stype_to_c(pt));
                buf_write(cg->buf, " ");
                buf_write(cg->buf, params->children[i]->value);
            }
        }
        buf_write(cg->buf, ")\n{\n");

        cg->indent++;
        if (node->children_count > 1)
            gen_stmt(cg, node->children[1]);
        cg->indent--;
        cg_scope_exit(cg);
        buf_write(cg->buf, "}\n\n");
        break;
    }

    case NODE_RETURN:
        write_indent(cg);
        buf_write(cg->buf, "return");
        if (node->children_count > 0)
        {
            buf_write(cg->buf, " ");
            gen_expr(cg, node->children[0]);
        }
        buf_write(cg->buf, ";\n");
        break;

    case NODE_BREAK:
        write_indent(cg);
        buf_write(cg->buf, "break;\n");
        break;

    case NODE_CONTINUE:
        write_indent(cg);
        buf_write(cg->buf, "continue;\n");
        break;

    case NODE_PASS:
        break;

    case NODE_FUNCTION_CALL:
    {
        char tmp_names[64][32];
        gen_concat_temps(cg, node->children, node->children_count, tmp_names, 64);
        write_indent(cg);
        buf_write(cg->buf, node->value);
        buf_write(cg->buf, "(");
        gen_args(cg, node->children, node->children_count, tmp_names, 64);
        buf_write(cg->buf, ");\n");
        break;
    }

    default:
        write_indent(cg);
        gen_expr(cg, node);
        buf_write(cg->buf, ";\n");
        break;
    }
}

/* Walks the typed AST and produces a complete C source file as a heap-allocated string.
   Function definitions are written to a separate buffer so they appear before main().
   A single CodeGen with one current_scope cursor is used so scope navigation order
   matches the semantic analyzer's traversal order exactly.
   Caller must free the returned string. Returns NULL on allocation failure. */
char *generate_code(const ASTNode *root, SymbolTable *global_scope, ErrorList *errors)
{
    if (!root)
        return NULL;

    CodeBuf fn_buf, main_buf;
    if (buf_init(&fn_buf) != 0 || buf_init(&main_buf) != 0)
    {
        error_list_add_staged(errors, "Memory allocation failed in code generator", 0, 0, STAGE_CODEGEN);
        buf_free(&fn_buf);
        return NULL;
    }

    // Single CodeGen — fn_buf and main_buf share one scope cursor so next_child
    // advances in AST order regardless of which buffer is active.
    CodeGen cg;
    cg.current_scope = global_scope;
    cg.indent        = 1;
    cg.tmp_counter   = 0;
    cg.errors        = errors;
    cg.buf           = &main_buf;

    if (root->type == NODE_PROGRAM)
    {
        for (int i = 0; i < root->children_count; i++)
        {
            ASTNode *child = root->children[i];
            if (child && child->type == NODE_DEF)
            {
                cg.buf    = &fn_buf;
                cg.indent = 0;
            }
            else
            {
                cg.buf    = &main_buf;
                cg.indent = 1;
            }
            gen_stmt(&cg, child);
        }
    }

    CodeBuf out;
    if (buf_init(&out) != 0)
    {
        error_list_add_staged(errors, "Memory allocation failed in code generator", 0, 0, STAGE_CODEGEN);
        buf_free(&fn_buf);
        buf_free(&main_buf);
        return NULL;
    }

    buf_write(&out, "#include <stdio.h>\n");
    buf_write(&out, "#include <math.h>\n\n");
    if (fn_buf.len > 0)
        buf_write(&out, fn_buf.data);
    buf_write(&out, "int main(void)\n{\n");
    if (main_buf.len > 0)
        buf_write(&out, main_buf.data);
    buf_write(&out, "    return 0;\n}\n");

    buf_free(&fn_buf);
    buf_free(&main_buf);

    return out.data; // caller must free
}

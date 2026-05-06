/* code_generator.c
 *
 * Walks the typed AST and produces C source text. Function definitions
 * go into a separate buffer so they appear before main() in the output.
 * Expressions are written inline; statements write full lines with newlines.
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
    b->len  = b->cap = 0;
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
    b->len         += slen;
    b->data[b->len] = '\0';
    return 0;
}

/* Tracks which variables have been declared in the current C scope.
   First assignment → emit type + name. Subsequent ones → just name. */
#define DECL_SIZE 128

typedef struct { char name[256]; int used; } DeclEntry;
typedef struct { DeclEntry entries[DECL_SIZE]; } DeclTable;

static void decl_init(DeclTable *t)
{
    for (int i = 0; i < DECL_SIZE; i++)
        t->entries[i].used = 0;
}

static unsigned int decl_hash(const char *s)
{
    unsigned int h = 0;
    while (*s)
        h = h * 31 + (unsigned char)*s++;
    return h;
}

static int decl_contains(const DeclTable *t, const char *name)
{
    unsigned int index = decl_hash(name) & (DECL_SIZE - 1);
    int probes = 0;
    while (t->entries[index].used && probes < DECL_SIZE)
    {
        if (strcmp(t->entries[index].name, name) == 0)
            return 1;
        index = (index + 1) & (DECL_SIZE - 1);
        probes++;
    }
    return 0;
}

static void decl_mark(DeclTable *t, const char *name)
{
    unsigned int index = decl_hash(name) & (DECL_SIZE - 1);
    int probes = 0;
    while (t->entries[index].used &&
           strcmp(t->entries[index].name, name) != 0 &&
           probes < DECL_SIZE)
    {
        index = (index + 1) & (DECL_SIZE - 1);
        probes++;
    }
    strncpy(t->entries[index].name, name, 255);
    t->entries[index].name[255] = '\0';
    t->entries[index].used = 1;
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

typedef struct
{
    CodeBuf   *buf;
    DeclTable *decl;
    int        indent;
    ErrorList *errors;
} CG;

static void write_indent(CG *cg)
{
    for (int i = 0; i < cg->indent; i++)
        buf_write(cg->buf, "    ");
}

static void gen_expr(CG *cg, const ASTNode *node);
static void gen_stmt(CG *cg, const ASTNode *node);

/* Writes an expression inline to the buffer — no indentation, no newline.
   Called recursively for sub-expressions (binary ops, function call args, etc.). */
static void gen_expr(CG *cg, const ASTNode *node)
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
        const char *c_op = op;
        if      (strcmp(op, "and") == 0) c_op = "&&";
        else if (strcmp(op, "or")  == 0) c_op = "||";
        else if (strcmp(op, "//")  == 0) c_op = "/";

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
        if (node->value[0] != '\0')
            buf_write(cg->buf, node->value);
        break;
    }
}

/* Writes a full statement to the buffer, including indentation and trailing newline.
   Handles all statement node types; delegates expressions to gen_expr. */
static void gen_stmt(CG *cg, const ASTNode *node)
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

        write_indent(cg);
        if (!decl_contains(cg->decl, target->value))
        {
            SemanticType t = node->inferred_type != STYPE_UNKNOWN
                             ? node->inferred_type : STYPE_INT;
            buf_write(cg->buf, stype_to_c(t));
            buf_write(cg->buf, " ");
            buf_write(cg->buf, target->value);
            buf_write(cg->buf, " = ");
            decl_mark(cg->decl, target->value);
        }
        else
        {
            buf_write(cg->buf, target->value);
            buf_write(cg->buf, " = ");
        }
        gen_expr(cg, value);
        buf_write(cg->buf, ";\n");
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

        const char *op = node->type == NODE_ASSIGN_PLUS  ? "+=" :
                         node->type == NODE_ASSIGN_MINUS ? "-=" :
                         node->type == NODE_ASSIGN_MULT  ? "*=" : "/=";
        write_indent(cg);
        buf_write(cg->buf, target->value);
        buf_write(cg->buf, " ");
        buf_write(cg->buf, op);
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
        if (node->children_count > 1)
            gen_stmt(cg, node->children[1]);
        cg->indent--;
        write_indent(cg);
        buf_write(cg->buf, "}");
        // elif/else nodes are extra children of if, emitted inline
        for (int i = 2; i < node->children_count; i++)
            gen_stmt(cg, node->children[i]);
        buf_write(cg->buf, "\n");
        break;
    }

    case NODE_ELIF:
    {
        buf_write(cg->buf, " else if (");
        gen_expr(cg, node->children_count > 0 ? node->children[0] : NULL);
        buf_write(cg->buf, ") {\n");
        cg->indent++;
        if (node->children_count > 1)
            gen_stmt(cg, node->children[1]);
        cg->indent--;
        write_indent(cg);
        buf_write(cg->buf, "}");
        for (int i = 2; i < node->children_count; i++)
            gen_stmt(cg, node->children[i]);
        break;
    }

    case NODE_ELSE:
    {
        buf_write(cg->buf, " else {\n");
        cg->indent++;
        for (int i = 0; i < node->children_count; i++)
            gen_stmt(cg, node->children[i]);
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
        if (node->children_count > 1)
            gen_stmt(cg, node->children[1]);
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
        gen_stmt(cg, body);
        cg->indent--;
        write_indent(cg);
        buf_write(cg->buf, "}\n");
        break;
    }

    case NODE_PRINT:
    {
        write_indent(cg);
        if (node->children_count == 0)
        {
            buf_write(cg->buf, "printf(\"\\n\");\n");
            break;
        }
        buf_write(cg->buf, "printf(\"");
        for (int i = 0; i < node->children_count; i++)
        {
            if (i > 0)
                buf_write(cg->buf, " ");
            buf_write(cg->buf, stype_to_fmt(node->children[i]->inferred_type));
        }
        buf_write(cg->buf, "\\n\"");
        for (int i = 0; i < node->children_count; i++)
        {
            buf_write(cg->buf, ", ");
            gen_expr(cg, node->children[i]);
        }
        buf_write(cg->buf, ");\n");
        break;
    }

    case NODE_DEF:
    {
        // child[0]=param list, child[1]=body block
        // all functions default to int return type since Python has no annotations
        buf_write(cg->buf, "int ");
        buf_write(cg->buf, node->value);
        buf_write(cg->buf, "(");

        if (node->children_count > 0 &&
            node->children[0]->type == NODE_PARAM_LIST)
        {
            const ASTNode *params = node->children[0];
            for (int i = 0; i < params->children_count; i++)
            {
                if (i > 0)
                    buf_write(cg->buf, ", ");
                SemanticType pt = params->children[i]->inferred_type;
                buf_write(cg->buf, stype_to_c(pt != STYPE_UNKNOWN ? pt : STYPE_INT));
                buf_write(cg->buf, " ");
                buf_write(cg->buf, params->children[i]->value);
            }
        }
        buf_write(cg->buf, ")\n{\n");

        // push a fresh declaration scope for the function body
        DeclTable  fn_decl;
        DeclTable *saved = cg->decl;
        decl_init(&fn_decl);
        cg->decl = &fn_decl;
        cg->indent++;
        if (node->children_count > 1)
            gen_stmt(cg, node->children[1]);
        cg->indent--;
        cg->decl = saved;
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

    default:
        write_indent(cg);
        gen_expr(cg, node);
        buf_write(cg->buf, ";\n");
        break;
    }
}

/* Walks the typed AST and produces a complete C source file as a heap-allocated string.
   Function definitions are written to a separate buffer so they appear before main().
   Caller must free the returned string. Returns NULL on allocation failure. */
char *generate_code(const ASTNode *root, ErrorList *errors)
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

    DeclTable main_decl;
    decl_init(&main_decl);

    CG fn_cg   = { &fn_buf,   &main_decl, 0, errors };
    CG main_cg = { &main_buf, &main_decl, 1, errors };

    if (root->type == NODE_PROGRAM)
    {
        for (int i = 0; i < root->children_count; i++)
        {
            ASTNode *child = root->children[i];
            if (child && child->type == NODE_DEF)
                gen_stmt(&fn_cg, child);
            else
                gen_stmt(&main_cg, child);
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

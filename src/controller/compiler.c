/* compiler.c */

#include "compiler.h"
#include "../Model/errors/errors.h"
#include "../Model/lexer/lexer.h"
#include "../Model/parser/parser.h"
#include "../Model/Symantic analayzer/symantic_analyzer.h"
#include "../Model/code generator/code_generator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Token list                                                           */
/* ------------------------------------------------------------------ */

typedef struct
{
    Token *tokens;
    int count;
    int capacity;
} TokenList;

static int token_list_init(TokenList *list)
{
    list->capacity = 256;
    list->count = 0;
    list->tokens = malloc(sizeof(Token) * (size_t)list->capacity);
    return list->tokens ? 0 : -1;
}

static int token_list_append(TokenList *list, Token t)
{
    if (list->count >= list->capacity)
    {
        int new_cap = list->capacity * 2;
        Token *new_tokens = realloc(list->tokens, sizeof(Token) * (size_t)new_cap);
        if (!new_tokens)
            return -1;
        list->tokens = new_tokens;
        list->capacity = new_cap;
    }
    list->tokens[list->count++] = t;
    return 0;
}

static void token_list_free(TokenList *list)
{
    free(list->tokens);
    list->tokens = NULL;
    list->count = 0;
    list->capacity = 0;
}

/* ------------------------------------------------------------------ */
/* AST printer                                                          */
/* ------------------------------------------------------------------ */

static const char *node_type_name(NodeType type)
{
    switch (type)
    {
    case NODE_PROGRAM:
        return "PROGRAM";
    case NODE_ASSIGN:
        return "ASSIGN";
    case NODE_ASSIGN_PLUS:
        return "ASSIGN_PLUS";
    case NODE_ASSIGN_MINUS:
        return "ASSIGN_MINUS";
    case NODE_ASSIGN_MULT:
        return "ASSIGN_MULT";
    case NODE_ASSIGN_DIV:
        return "ASSIGN_DIV";
    case NODE_IF:
        return "IF";
    case NODE_ELIF:
        return "ELIF";
    case NODE_ELSE:
        return "ELSE";
    case NODE_WHILE:
        return "WHILE";
    case NODE_FOR:
        return "FOR";
    case NODE_DEF:
        return "DEF";
    case NODE_RETURN:
        return "RETURN";
    case NODE_BREAK:
        return "BREAK";
    case NODE_CONTINUE:
        return "CONTINUE";
    case NODE_PASS:
        return "PASS";
    case NODE_PRINT:
        return "PRINT";
    case NODE_BLOCK:
        return "BLOCK";
    case NODE_BINARY_OP:
        return "BINARY_OP";
    case NODE_UNARY_OP:
        return "UNARY_OP";
    case NODE_CALL:
        return "CALL";
    case NODE_SUBSCRIPT:
        return "SUBSCRIPT";
    case NODE_IDENTIFIER:
        return "IDENTIFIER";
    case NODE_INT_LITERAL:
        return "INT_LITERAL";
    case NODE_FLOAT_LITERAL:
        return "FLOAT_LITERAL";
    case NODE_STRING_LITERAL:
        return "STRING_LITERAL";
    case NODE_BOOL_LITERAL:
        return "BOOL_LITERAL";
    case NODE_NONE_LITERAL:
        return "NONE_LITERAL";
    case NODE_PARAM_LIST:
        return "PARAM_LIST";
    case NODE_ARG_LIST:
        return "ARG_LIST";
    case NODE_FUNCTION_CALL:
        return "FUNCTION_CALL";
    case NODE_RANGE:
        return "RANGE";
    default:
        return "UNKNOWN";
    }
}

/* Recursive pre-order helper.
   prefix  — the indentation string built up by parent calls
   is_last — whether this node is the last child of its parent        */
static void print_ast_node(const ASTNode *node,
                           const char *prefix, int is_last)
{
    /* Branch symbols: last child gets └── , others get ├──  */
    const char *branch = is_last ? "\xc2\xb8\xc2\xb8\xc2\xb8 " : "\xe2\x94\x9c\xe2\x94\x80\xe2\x94\x80 ";
    const char *padding = is_last ? "    " : "\xe2\x94\x82   ";

    /* Use plain ASCII fallback for Windows CMD compatibility */
    branch = is_last ? "`-- " : "|-- ";
    padding = is_last ? "    " : "|   ";

    /* Print this node */
    printf("%s%s%s", prefix, branch, node_type_name(node->type));
    if (node->value[0] != '\0')
        printf(" '%s'", node->value);
    printf("  [line %d, col %d]\n", node->line, node->column);

    /* Build the prefix for children */
    char new_prefix[512];
    int prefix_len = (int)strlen(prefix);
    int pad_len = (int)strlen(padding);
    if (prefix_len + pad_len < (int)sizeof(new_prefix) - 1)
    {
        memcpy(new_prefix, prefix, (size_t)prefix_len);
        memcpy(new_prefix + prefix_len, padding, (size_t)pad_len);
        new_prefix[prefix_len + pad_len] = '\0';
    }
    else
    {
        /* Prefix too deep — truncate gracefully */
        strncpy(new_prefix, prefix, sizeof(new_prefix) - 1);
        new_prefix[sizeof(new_prefix) - 1] = '\0';
    }

    for (int i = 0; i < node->children_count; i++)
        print_ast_node(node->children[i], new_prefix,
                       i == node->children_count - 1);
}

/* Prints the full AST rooted at node to stdout. */
static void print_ast(const ASTNode *node)
{
    if (!node)
    {
        printf("(empty tree)\n");
        return;
    }

    /* Print the root without a branch prefix */
    printf("%s", node_type_name(node->type));
    if (node->value[0] != '\0')
        printf(" '%s'", node->value);
    printf("  [line %d, col %d]\n", node->line, node->column);

    for (int i = 0; i < node->children_count; i++)
        print_ast_node(node->children[i], "",
                       i == node->children_count - 1);
}

/* ------------------------------------------------------------------ */
/* Pipeline stage stubs                                                 */
/* ------------------------------------------------------------------ */

static ASTNode *run_parser(const TokenList *token_list, ErrorList *errors)
{
    Parser parser;
    parser_init(&parser, token_list->tokens, token_list->count, errors);
    return parse_program(&parser);
}

static ASTNode *run_semantic_analyzer(ASTNode *ast, ErrorList *errors)
{
    return semantic_analyze(ast, errors);
}

static char *run_code_generator(ASTNode *typed_ast, ErrorList *errors)
{
    return generate_code(typed_ast, errors);
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

int compile(const char *source, const char *output_file)
{
    if (!source)
    {
        fprintf(stderr, "Error: cannot open input file\n");
        return -1;
    }

    /* Shared error list — passed through every stage */
    ErrorList errors;
    error_list_init(&errors);

    /* --- Stage 1: Lexer ---
       Runs the lexer over the source and collects all tokens into a list.
       TOKEN_ERROR tokens are recorded in the error list but do not stop
       the pipeline — the compiler continues to collect further errors. */
    Lexer lexer;
    lexer_init(&lexer, source, &errors);

    TokenList token_list;
    if (token_list_init(&token_list) != 0)
    {
        error_list_add(&errors, "Memory allocation failed for token list", 0, 0);
        error_list_write_log(&errors);
        return -1;
    }

    Token tok;
    do
    {
        tok = lexer_next_token(&lexer);

        if (tok.type == TOKEN_ERROR)
            error_list_add(&errors, tok.value, tok.line, tok.column);

        if (token_list_append(&token_list, tok) != 0)
        {
            error_list_add(&errors, "Memory allocation failed during tokenization", 0, 0);
            break;
        }

        printf("Token: type=%d, value='%s', line=%d, column=%d\n", tok.type, tok.value, tok.line, tok.column);
    } while (tok.type != TOKEN_EOF);

    /* --- Stage 2: Parser ---
       Receives the token list, returns an AST. */
    ASTNode *ast = run_parser(&token_list, &errors);
    token_list_free(&token_list);

    /* --- Debug: print the AST --- */
    printf("\n--- AST ---\n");
    print_ast(ast);
    printf("-----------\n\n");

    /* --- Stage 3: Semantic analysis ---
       Receives the AST and a fresh symbol table; returns a typed AST. */
    ASTNode *typed_ast = run_semantic_analyzer(ast, &errors);

    /* --- Debug: print the AST --- */
    printf("\n--- AST ---\n");
    print_ast(typed_ast);
    printf("-----------\n\n");

    /* --- Stage 4: Code generation ---
       Receives the typed AST, returns heap-allocated C source text. */
    char *c_code = run_code_generator(typed_ast, &errors);

    /* --- Write C output file --- */
    FILE *out = fopen(output_file, "w");
    if (!out)
    {
        error_list_add(&errors, "Cannot open output file for writing", 0, 0);
    }
    else
    {
        if (c_code)
            fputs(c_code, out);
        fclose(out);
    }
    free(c_code);

    /* --- Write error log ---
       Always writes errors.log: either the error list or a success message. */
    error_list_write_log(&errors);

    return errors.count > 0 ? -1 : 0;
}

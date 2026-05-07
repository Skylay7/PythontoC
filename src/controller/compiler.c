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

// Simple growable array that collects tokens from the lexer before passing them to the parser.
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

// Debug tree printer — prints the AST to stdout in a tree format for inspection.
// node_type_name maps enum values to readable strings; print_ast_node recurses per child.
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

/* Recursive tree printer. prefix is the indentation built up from parent calls. */
static void print_ast_node(const ASTNode *node, const char *prefix, int is_last)
{
    // ASCII fallback — Unicode box-drawing breaks on Windows CMD
    const char *branch = is_last ? "`-- " : "|-- ";
    const char *padding = is_last ? "    " : "|   ";

    printf("%s%s%s", prefix, branch, node_type_name(node->type));
    if (node->value[0] != '\0')
        printf(" '%s'", node->value);
    printf("  [line %d, col %d]\n", node->line, node->column);

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
        // prefix got too long somehow, just truncate
        strncpy(new_prefix, prefix, sizeof(new_prefix) - 1);
        new_prefix[sizeof(new_prefix) - 1] = '\0';
    }

    for (int i = 0; i < node->children_count; i++)
        print_ast_node(node->children[i], new_prefix, i == node->children_count - 1);
}

static void print_ast(const ASTNode *node)
{
    if (!node)
    {
        printf("(empty tree)\n");
        return;
    }

    printf("%s", node_type_name(node->type));
    if (node->value[0] != '\0')
        printf(" '%s'", node->value);
    printf("  [line %d, col %d]\n", node->line, node->column);

    for (int i = 0; i < node->children_count; i++)
        print_ast_node(node->children[i], "", i == node->children_count - 1);
}

// Thin wrappers that call into each pipeline stage with the right arguments.
static ASTNode *run_parser(const TokenList *token_list, ErrorList *errors)
{
    Parser parser;
    parser_init(&parser, token_list->tokens, token_list->count, errors);
    return parse_program(&parser);
}

static ASTNode *run_semantic_analyzer(ASTNode *ast, ErrorList *errors, SymbolTable **out_scope)
{
    return semantic_analyze(ast, errors, out_scope);
}

static char *run_code_generator(ASTNode *typed_ast, SymbolTable *global_scope, ErrorList *errors)
{
    return generate_code(typed_ast, global_scope, errors);
}

/* Runs the full compiler pipeline on source and writes C output to output_file.
   Errors from all stages accumulate in one list and are written to errors.log at the end.
   Returns 0 on success, -1 if any errors were recorded. */
int compile(const char *source, const char *output_file)
{
    if (!source)
    {
        fprintf(stderr, "Error: cannot open input file\n");
        return -1;
    }

    ErrorList errors;
    error_list_init(&errors);

    // stage 1: lexer
    Lexer lexer;
    lexer_init(&lexer, source, &errors);

    TokenList token_list;
    if (token_list_init(&token_list) != 0)
    {
        error_list_add_staged(&errors, "Memory allocation failed for token list", 0, 0, STAGE_LEXER);
        error_list_write_log(&errors);
        return -1;
    }

    Token tok;
    do
    {
        tok = lexer_next_token(&lexer);

        if (tok.type == TOKEN_ERROR)
            error_list_add_staged(&errors, tok.value, tok.line, tok.column, STAGE_LEXER);

        if (token_list_append(&token_list, tok) != 0)
        {
            error_list_add_staged(&errors, "Memory allocation failed during tokenization", 0, 0, STAGE_LEXER);
            break;
        }

        printf("Token: type=%d, value='%s', line=%d, column=%d\n", tok.type, tok.value, tok.line, tok.column);
    } while (tok.type != TOKEN_EOF);

    // stage 2: parse
    ASTNode *ast = run_parser(&token_list, &errors);
    token_list_free(&token_list);

    printf("\n--- AST ---\n");
    print_ast(ast);
    printf("-----------\n\n");

    // stage 3: semantic analysis
    SymbolTable *global_scope = NULL;
    ASTNode *typed_ast = run_semantic_analyzer(ast, &errors, &global_scope);

    printf("\n--- typed AST ---\n");
    print_ast(typed_ast);
    printf("-----------\n\n");

    // stage 4: code generation
    char *c_code = run_code_generator(typed_ast, global_scope, &errors);

    FILE *out = fopen(output_file, "w");
    if (!out)
    {
        error_list_add_staged(&errors, "Cannot open output file for writing", 0, 0, STAGE_GENERAL);
    }
    else
    {
        if (c_code)
            fputs(c_code, out);
        fclose(out);
    }
    free(c_code);
    symbol_table_free_tree(global_scope);

    error_list_write_log(&errors);

    return errors.count > 0 ? -1 : 0;
}

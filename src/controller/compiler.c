/* compiler.c */

#include "compiler.h"
#include "../Model/errors/errors.h"
#include "../Model/lexer/lexer.h"
#include <stdio.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/* Token list                                                           */
/* ------------------------------------------------------------------ */

typedef struct
{
    Token *tokens;
    int    count;
    int    capacity;
} TokenList;

static int token_list_init(TokenList *list)
{
    list->capacity = 256;
    list->count    = 0;
    list->tokens   = malloc(sizeof(Token) * (size_t)list->capacity);
    return list->tokens ? 0 : -1;
}

static int token_list_append(TokenList *list, Token t)
{
    if (list->count >= list->capacity)
    {
        int    new_cap    = list->capacity * 2;
        Token *new_tokens = realloc(list->tokens, sizeof(Token) * (size_t)new_cap);
        if (!new_tokens)
            return -1;
        list->tokens   = new_tokens;
        list->capacity = new_cap;
    }
    list->tokens[list->count++] = t;
    return 0;
}

static void token_list_free(TokenList *list)
{
    free(list->tokens);
    list->tokens   = NULL;
    list->count    = 0;
    list->capacity = 0;
}

/* ------------------------------------------------------------------ */
/* Pipeline stage stubs                                                 */
/* ------------------------------------------------------------------ */

/* TODO: Replace with real parser once implemented.
   Receives the token list and the shared error list.
   Returns an AST (currently NULL). */
static void *run_parser(const TokenList *tokens, ErrorList *errors)
{
    (void)tokens;
    (void)errors;
    return NULL;
}

/* TODO: Replace with real semantic analyzer once implemented.
   Receives the AST and the shared error list.
   Returns a typed AST (currently NULL). */
static void *run_semantic_analyzer(void *ast, ErrorList *errors)
{
    (void)ast;
    (void)errors;
    return NULL;
}

/* TODO: Replace with real code generator once implemented.
   Receives the typed AST and the shared error list.
   Returns a heap-allocated C code string (caller must free), or NULL. */
static char *run_code_generator(void *typed_ast, ErrorList *errors)
{
    (void)typed_ast;
    (void)errors;
    return NULL;
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
    lexer_init(&lexer, source);

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
    } while (tok.type != TOKEN_EOF);

    /* --- Stage 2: Parser ---
       Receives the token list, returns an AST. */
    void *ast = run_parser(&token_list, &errors);
    token_list_free(&token_list);

    /* --- Stage 3: Semantic analysis ---
       Receives the AST and a fresh symbol table; returns a typed AST. */
    void *typed_ast = run_semantic_analyzer(ast, &errors);

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

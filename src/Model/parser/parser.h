/* parser.h
 *
 * Parser — mirrors the Lexer struct in purpose.
 * It centralises all parser state so every recursive-descent function
 * receives a single pointer instead of a large parameter list.
 *
 * Fields:
 *   tokens   — flat array of tokens produced by the lexer
 *   position — index of the token currently being inspected
 *   count    — total number of tokens in the array
 *   errors   — shared error list (same instance passed through the pipeline)
 */

#ifndef PARSER_H
#define PARSER_H

#include "../lexer/token.h"
#include "../lexer/token_types.h"
#include "../errors/errors.h"
#include "ast_ds.h"

/* ------------------------------------------------------------------ */
/* Statement dispatch hash table                                        */
/* Maps TokenType → parse function via open-address hashing            */
/* ------------------------------------------------------------------ */

#define DISPATCH_TABLE_SIZE 32  /* power of 2, larger than # of statements */

struct Parser;
typedef struct ASTNode *(*ParseFn)(struct Parser *parser);

typedef struct
{
    TokenType key;
    ParseFn   fn;
    int       occupied;
} DispatchEntry;

typedef struct
{
    DispatchEntry entries[DISPATCH_TABLE_SIZE];
} DispatchTable;

/* ------------------------------------------------------------------ */
/* Parser state                                                         */
/* ------------------------------------------------------------------ */

typedef struct Parser
{
    const Token   *tokens;   /* token array from the lexer     */
    int            position; /* index of the current token     */
    int            count;    /* total number of tokens         */
    ErrorList     *errors;   /* shared error list — never NULL */
    DispatchTable  dispatch; /* keyword → parse function       */
} Parser;

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

/* Initialise the parser with a token array and a shared error list. */
void parser_init(Parser *parser,
                 const Token *tokens, int count,
                 ErrorList *errors);

/* Parse the full token stream and return the root NODE_PROGRAM node.
   Returns NULL only on a fatal allocation failure. */
ASTNode *parse_program(Parser *parser);

#endif

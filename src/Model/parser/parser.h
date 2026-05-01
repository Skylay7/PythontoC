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
/* Statement dispatch table                                             */
/* Flat array indexed by TokenType — NULL means no handler registered  */
/* ------------------------------------------------------------------ */

struct Parser;
typedef struct ASTNode *(*ParseFn)(struct Parser *parser);

/* ------------------------------------------------------------------ */
/* Parser state                                                         */
/* ------------------------------------------------------------------ */

typedef struct Parser
{
    const Token *tokens;                /* token array from the lexer        */
    int position;                       /* index of the current token        */
    int count;                          /* total number of tokens            */
    ErrorList *errors;                  /* shared error list — never NULL    */
    ParseFn stmt_dispatch[TOKEN_COUNT]; /* TokenType → parse_* function   */
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

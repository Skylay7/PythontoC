/* parser.h */

#ifndef PARSER_H
#define PARSER_H

#include "../lexer/token.h"
#include "../lexer/token_types.h"
#include "../errors/errors.h"
#include "ast_ds.h"

/* Hash table mapping TokenType → parse function.
   Using a hash table here instead of a switch so adding new statement
   types only requires one line in dispatch_init. */

#define DISPATCH_TABLE_SIZE 32

// Forward declaration for parse function pointer type.
struct Parser;
typedef struct ASTNode *(*ParseFn)(struct Parser *parser);

/* One slot in the open-addressed dispatch hash table. */
typedef struct
{
    TokenType key;  /* leading token that triggers this parse function */
    ParseFn fn;     /* the parse function to call for this token type */
    int occupied;   /* 1 if this slot holds a valid entry */
} DispatchEntry;

/* Hash table from leading TokenType to statement parse function. */
typedef struct
{
    DispatchEntry entries[DISPATCH_TABLE_SIZE];
} DispatchTable;

/* Parser state: a read cursor over a flat token array. */
typedef struct Parser
{
    const Token *tokens; /* token array produced by the lexer; not owned */
    int position;        /* index of the current (look-ahead) token */
    int count;           /* total number of tokens in the array */
    ErrorList *errors;   /* shared error list; not owned */
    DispatchTable dispatch; /* statement dispatch table initialised by parser_init */
} Parser;

void parser_init(Parser *parser, const Token *tokens, int count, ErrorList *errors);
ASTNode *parse_program(Parser *parser);

#endif

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

struct Parser;
typedef struct ASTNode *(*ParseFn)(struct Parser *parser);

typedef struct
{
    TokenType key;
    ParseFn   fn;
    int       occupied; // flag to indicate if this slot is used
} DispatchEntry;

typedef struct
{
    DispatchEntry entries[DISPATCH_TABLE_SIZE];
} DispatchTable;

typedef struct Parser
{
    const Token   *tokens;
    int            position;
    int            count;
    ErrorList     *errors;
    DispatchTable  dispatch;
} Parser;

void     parser_init(Parser *parser, const Token *tokens, int count, ErrorList *errors);
ASTNode *parse_program(Parser *parser);

#endif

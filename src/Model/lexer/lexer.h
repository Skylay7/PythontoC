/* lexer.h */

#ifndef LEXER_H
#define LEXER_H

#include "token.h"
#include "indent_stack.h"
#include "keyword_table.h"
#include "../errors/errors.h"

#define MAX_INDENT_DEPTH  128
#define LEXER_BUFFER_SIZE 256

typedef struct
{
    const char *source;
    int position;
    int length;

    int line;
    int column;

    IndentStack indent_stack;
    int pending_dedents; // how many DEDENT tokens are still waiting to be emitted
    int at_line_start;   // 1 if the next token starts a new logical line (triggers indent check)

    char buffer[LEXER_BUFFER_SIZE];
    int buffer_length;

    KeywordTable keyword_table;
    ErrorList   *errors;
} Lexer;

void  lexer_init(Lexer *lexer, const char *source, ErrorList *errors);
Token lexer_next_token(Lexer *lexer);

#endif

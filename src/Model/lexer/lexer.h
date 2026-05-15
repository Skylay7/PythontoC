/* lexer.h */

#ifndef LEXER_H
#define LEXER_H

#include "token.h"
#include "indent_stack.h"
#include "keyword_table.h"
#include "../errors/errors.h"

#define MAX_INDENT_DEPTH 128
#define LEXER_BUFFER_SIZE 256

/* Stateful lexer that converts a source string into a stream of Tokens. */
typedef struct
{
    const char *source; /* null-terminated Python source text */
    int position;       /* current read index into source */
    int length;         /* total length of source */

    int line;   /* current 1-based line number */
    int column; /* current 1-based column number */

    IndentStack indent_stack;
    int pending_dedents; /* how many DEDENT tokens are still waiting to be emitted */
    int at_line_start;   /* 1 if the next token starts a new logical line (triggers indent check) */

    char buffer[LEXER_BUFFER_SIZE]; /* scratch space for building token text */
    int buffer_length;

    KeywordTable keyword_table; /* maps keyword strings to their TokenType */
    ErrorList *errors;          /* shared error list; not owned */
} Lexer;

void lexer_init(Lexer *lexer, const char *source, ErrorList *errors);
Token lexer_next_token(Lexer *lexer);

#endif

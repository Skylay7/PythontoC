/* lexer.h */

#ifndef LEXER_H
#define LEXER_H

#include "token.h"
#include "indent_stack.h"
#include "keyword_table.h"

#define MAX_INDENT_DEPTH 128  /* maximum nesting depth */
#define LEXER_BUFFER_SIZE 256 /* buffer for building token values */

typedef struct
{
    /* --- Source code and position --- */
    const char *source; /* pointer to the source code string */
    int position;       /* current index in source (0-indexed) */
    int length;         /* total length of source */

    /* --- Line tracking for error messages --- */
    int line;   /* current line (1-indexed) */
    int column; /* current column (1-indexed) */

    /* --- Indentation tracking (the PDA stack) --- */
    IndentStack indent_stack; /* stack of indentation levels */
    int pending_dedents;      /* DEDENT tokens still to emit */
    int at_line_start;        /* flag: are we at the start of a line? */

    /* --- Buffer for building multi-character tokens --- */
    char buffer[LEXER_BUFFER_SIZE];
    int buffer_length;

    /* --- Keyword lookup table --- */
    KeywordTable keyword_table;
} Lexer;

/* Public functions */
void lexer_init(Lexer *lexer, const char *source);
Token lexer_next_token(Lexer *lexer);

#endif
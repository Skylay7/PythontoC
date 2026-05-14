/* token.h */

#ifndef TOKEN_H
#define TOKEN_H

#include "token_types.h"

#define MAX_TOKEN_VALUE 256

/* A single lexical unit produced by the lexer. */
typedef struct
{
    TokenType type;              /* category of the token */
    char value[MAX_TOKEN_VALUE]; /* raw text from the source */
    int line;                    /* 1-based source line */
    int column;                  /* 1-based source column */
} Token;

#endif

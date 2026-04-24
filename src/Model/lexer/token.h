/* token.h */

#ifndef TOKEN_H
#define TOKEN_H

#include "token_types.h"

#define MAX_TOKEN_VALUE 256 /* maximum length of a token's text */

typedef struct
{
    TokenType type;              /* the category of the token */
    char value[MAX_TOKEN_VALUE]; /* the actual text */
    int line;                    /* line number (1-indexed) */
    int column;                  /* column number (1-indexed) */
} Token;

#endif
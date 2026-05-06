/* token.h */

#ifndef TOKEN_H
#define TOKEN_H

#include "token_types.h"

#define MAX_TOKEN_VALUE 256

typedef struct
{
    TokenType type;
    char      value[MAX_TOKEN_VALUE];
    int       line;
    int       column;
} Token;

#endif

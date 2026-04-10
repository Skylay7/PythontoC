/* keyword_table.h */

#ifndef KEYWORD_TABLE_H
#define KEYWORD_TABLE_H

#include "token_types.h"

#define KEYWORD_TABLE_SIZE 64
#define MAX_KEYWORD_LENGTH 16

typedef struct
{
    char keyword[MAX_KEYWORD_LENGTH]; // the keyword text ("if", "while")
    TokenType token_type;             // the corresponding token type (TOKEN_IF, TOKEN_WHILE)
    int occupied;                     // flag to indicate if this slot is used
} KeywordEntry;

typedef struct
{
    KeywordEntry entries[KEYWORD_TABLE_SIZE];
} KeywordTable;

void keyword_table_init(KeywordTable *table);
TokenType keyword_table_lookup(const KeywordTable *table, const char *word);

#endif
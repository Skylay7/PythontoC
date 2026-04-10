/* keyword_table.c */

#include "keyword_table.h"
#include <string.h>

static unsigned int hash_string(const char *s)
{
    unsigned int hash = 0;
    while (*s)
    {
        hash = hash * 31 + (unsigned char)*s;
        s++;
    }
    return hash;
}

static void keyword_table_insert(KeywordTable *table,
                                 const char *keyword,
                                 TokenType token_type)
{
    unsigned int index = hash_string(keyword) & (KEYWORD_TABLE_SIZE - 1);

    while (table->entries[index].occupied)
    {
        index = (index + 1) & (KEYWORD_TABLE_SIZE - 1);
    }

    strncpy(table->entries[index].keyword, keyword, MAX_KEYWORD_LENGTH - 1);
    table->entries[index].keyword[MAX_KEYWORD_LENGTH - 1] = '\0';
    table->entries[index].token_type = token_type;
    table->entries[index].occupied = 1;
}

void keyword_table_init(KeywordTable *table)
{
    for (int i = 0; i < KEYWORD_TABLE_SIZE; i++)
    {
        table->entries[i].occupied = 0;
    }

    keyword_table_insert(table, "if", TOKEN_IF);
    keyword_table_insert(table, "elif", TOKEN_ELIF);
    keyword_table_insert(table, "else", TOKEN_ELSE);
    keyword_table_insert(table, "while", TOKEN_WHILE);
    keyword_table_insert(table, "for", TOKEN_FOR);
    keyword_table_insert(table, "def", TOKEN_DEF);
    keyword_table_insert(table, "return", TOKEN_RETURN);
    keyword_table_insert(table, "and", TOKEN_AND);
    keyword_table_insert(table, "or", TOKEN_OR);
    keyword_table_insert(table, "not", TOKEN_NOT);
    keyword_table_insert(table, "in", TOKEN_IN);
    keyword_table_insert(table, "True", TOKEN_TRUE);
    keyword_table_insert(table, "False", TOKEN_FALSE);
    keyword_table_insert(table, "None", TOKEN_NONE);
    keyword_table_insert(table, "print", TOKEN_PRINT);
    keyword_table_insert(table, "break", TOKEN_BREAK);
    keyword_table_insert(table, "continue", TOKEN_CONTINUE);
    keyword_table_insert(table, "pass", TOKEN_PASS);
}

TokenType keyword_table_lookup(const KeywordTable *table, const char *word)
{
    unsigned int index = hash_string(word) & (KEYWORD_TABLE_SIZE - 1);

    int probes = 0;
    while (table->entries[index].occupied && probes < KEYWORD_TABLE_SIZE)
    {
        if (strcmp(table->entries[index].keyword, word) == 0)
        {
            return table->entries[index].token_type;
        }
        index = (index + 1) & (KEYWORD_TABLE_SIZE - 1);
        probes++;
    }

    return TOKEN_IDENTIFIER;
}
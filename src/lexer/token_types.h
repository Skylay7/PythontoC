/* token_types.h */

#ifndef TOKEN_TYPES_H
#define TOKEN_TYPES_H

typedef enum
{
    /* Literals */
    TOKEN_INT_LITERAL,
    TOKEN_FLOAT_LITERAL,
    TOKEN_STRING_LITERAL,

    /* Identifier (user-defined name) */
    TOKEN_IDENTIFIER,

    /* Keywords */
    TOKEN_IF,
    TOKEN_ELIF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_FOR,
    TOKEN_DEF,
    TOKEN_RETURN,
    TOKEN_AND,
    TOKEN_OR,
    TOKEN_NOT,
    TOKEN_IN,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NONE,
    TOKEN_PRINT,
    TOKEN_BREAK,
    TOKEN_CONTINUE,
    TOKEN_PASS,

    /* Arithmetic operators */
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,
    TOKEN_DOUBLE_STAR,
    TOKEN_DOUBLE_SLASH,

    /* Comparison operators */
    TOKEN_EQ,
    TOKEN_NEQ,
    TOKEN_LT,
    TOKEN_GT,
    TOKEN_LTE,
    TOKEN_GTE,

    /* Assignment operators */
    TOKEN_ASSIGN,
    TOKEN_PLUS_ASSIGN,
    TOKEN_MINUS_ASSIGN,

    /* Delimiters */
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_COLON,
    TOKEN_COMMA,
    TOKEN_DOT,

    /* Structural tokens */
    TOKEN_NEWLINE,
    TOKEN_INDENT,
    TOKEN_DEDENT,

    /* Special */
    TOKEN_EOF,
    TOKEN_ERROR,

    TOKEN_COUNT /* equals total number of token types */
} TokenType;

#endif
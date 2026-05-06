/* node_types.h */

#ifndef NODE_TYPES_H
#define NODE_TYPES_H

typedef enum
{
    NODE_PROGRAM,

    NODE_ASSIGN,
    NODE_ASSIGN_PLUS,
    NODE_ASSIGN_MINUS,
    NODE_ASSIGN_MULT,
    NODE_ASSIGN_DIV,
    NODE_IF,
    NODE_ELIF,
    NODE_ELSE,
    NODE_WHILE,
    NODE_FOR,
    NODE_DEF,
    NODE_RETURN,
    NODE_BREAK,
    NODE_CONTINUE,
    NODE_PASS,
    NODE_PRINT,
    NODE_BLOCK,

    NODE_BINARY_OP,  // value holds the operator string
    NODE_UNARY_OP,
    NODE_CALL,
    NODE_SUBSCRIPT,

    NODE_IDENTIFIER,
    NODE_INT_LITERAL,
    NODE_FLOAT_LITERAL,
    NODE_STRING_LITERAL,
    NODE_BOOL_LITERAL,
    NODE_NONE_LITERAL,

    NODE_PARAM_LIST,
    NODE_ARG_LIST,
    NODE_FUNCTION_CALL, // value = function name, children = args

    NODE_RANGE, // only child is the upper bound expression

    NODE_COUNT
} NodeType;

#endif

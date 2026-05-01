#ifndef NODE_TYPES_H
#define NODE_TYPES_H

typedef enum
{
    /* --- Root --- */
    NODE_PROGRAM, /* top-level sequence of statements           */

    /* --- Statements --- */
    NODE_ASSIGN,       /* x = expr                                   */
    NODE_ASSIGN_PLUS,  /* x += expr                                  */
    NODE_ASSIGN_MINUS, /* x -= expr                                  */
    NODE_IF,           /* if / elif / else chain                     */
    NODE_ELIF,         /* elif branch (child of NODE_IF)             */
    NODE_ELSE,         /* else branch (child of NODE_IF / NODE_ELIF) */
    NODE_WHILE,        /* while loop                                 */
    NODE_FOR,          /* for loop                                   */
    NODE_DEF,          /* function definition                        */
    NODE_RETURN,       /* return statement                           */
    NODE_BREAK,        /* break statement                            */
    NODE_CONTINUE,     /* continue statement                         */
    NODE_PASS,         /* pass statement                             */
    NODE_PRINT,        /* print( ... )                               */
    NODE_BLOCK,        /* indented block — ordered list of stmts     */

    /* --- Expressions --- */
    NODE_BINARY_OP, /* left <op> right  (value holds operator)    */
    NODE_UNARY_OP,  /* <op> operand     (value holds operator)    */
    NODE_CALL,      /* func(args...)                              */
    NODE_SUBSCRIPT, /* obj[index]                                 */

    /* --- Leaves (terminals) --- */
    NODE_IDENTIFIER,     /* variable / function name  (value = name)  */
    NODE_INT_LITERAL,    /* integer constant          (value = text)  */
    NODE_FLOAT_LITERAL,  /* float constant            (value = text)  */
    NODE_STRING_LITERAL, /* string constant           (value = text)  */
    NODE_BOOL_LITERAL,   /* True / False              (value = text)  */
    NODE_NONE_LITERAL,   /* None                                       */

    /* --- Parameter / argument lists --- */
    NODE_PARAM_LIST, /* formal parameters of a def                */
    NODE_ARG_LIST,   /* actual arguments of a call                */

    /* --- Range (for-loop iterable) --- */
    NODE_RANGE, /* range(expr) — single child is upper bound */

    NODE_COUNT /* total number of node types (keep last)    */
} NodeType;

#endif

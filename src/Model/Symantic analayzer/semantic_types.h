/* semantic_types.h — inferred value types attached to AST nodes and symbols. */

#ifndef SEMANTIC_TYPES_H
#define SEMANTIC_TYPES_H

/* The resolved type of an expression, variable, or function return value. */
typedef enum
{
    STYPE_UNKNOWN = 0, /* not yet resolved                              */
    STYPE_INT,         /* Python int  → C int                          */
    STYPE_FLOAT,       /* Python float → C double                      */
    STYPE_STRING,      /* Python str  → C char*                        */
    STYPE_BOOL,        /* Python bool → C int (0/1)                    */
    STYPE_NONE,        /* Python None → void                           */
    STYPE_FUNCTION,    /* callable defined with def                    */
    STYPE_ERROR        /* type error detected during analysis           */
} SemanticType;

#endif
#ifndef SEMANTIC_TYPES_H
#define SEMANTIC_TYPES_H

typedef enum
{
    STYPE_UNKNOWN = 0, /* not yet resolved                              */
    STYPE_INT,
    STYPE_FLOAT,
    STYPE_STRING,
    STYPE_BOOL,
    STYPE_NONE,
    STYPE_FUNCTION,
    STYPE_LIST,
    STYPE_ERROR        /* type error detected during analysis           */
} SemanticType;

#endif
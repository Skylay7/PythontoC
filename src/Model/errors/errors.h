/* errors.h */

#ifndef ERRORS_H
#define ERRORS_H

#define MAX_ERRORS 256
#define MAX_ERROR_MSG 256

/* Which compiler phase produced the error. */
typedef enum
{
    STAGE_LEXER,    /* tokenisation */
    STAGE_PARSER,   /* syntax analysis */
    STAGE_SEMANTIC, /* type / scope checking */
    STAGE_CODEGEN,  /* C code emission */
    STAGE_GENERAL   /* not tied to a specific stage */
} CompileStage;

/* A single diagnostic with location and phase information. */
typedef struct
{
    char message[MAX_ERROR_MSG]; /* human-readable description */
    int line;                    /* 1-based source line */
    int column;                  /* 1-based source column */
    CompileStage stage;          /* phase that raised this error */
} CompileError;

/* Bounded collection of diagnostics accumulated during compilation. */
typedef struct
{
    CompileError errors[MAX_ERRORS]; /* fixed-size error buffer */
    int count;                       /* number of errors stored so far */
} ErrorList;

void error_list_init(ErrorList *list);
void error_list_add(ErrorList *list, const char *message, int line, int column);
void error_list_add_staged(ErrorList *list, const char *message, int line, int column, CompileStage stage);
void error_list_write_log(const ErrorList *list);

#endif

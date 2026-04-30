/* errors.h */

#ifndef ERRORS_H
#define ERRORS_H

#define MAX_ERRORS    256
#define MAX_ERROR_MSG 256

typedef struct
{
    char message[MAX_ERROR_MSG];
    int  line;
    int  column;
} CompileError;

typedef struct
{
    CompileError errors[MAX_ERRORS];
    int          count;
} ErrorList;

void error_list_init(ErrorList *list);
void error_list_add(ErrorList *list, const char *message, int line, int column);
void error_list_write_log(const ErrorList *list);

#endif

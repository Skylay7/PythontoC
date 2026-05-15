/* errors.c */

#include "errors.h"
#include <stdio.h>
#include <string.h>

void error_list_init(ErrorList *list)
{
    list->count = 0;
}

void error_list_add(ErrorList *list, const char *message, int line, int column)
{
    error_list_add_staged(list, message, line, column, STAGE_GENERAL);
}

void error_list_add_staged(ErrorList *list, const char *message, int line, int column, CompileStage stage)
{
    if (list->count >= MAX_ERRORS)
        return;
    strncpy(list->errors[list->count].message, message, MAX_ERROR_MSG - 1);
    list->errors[list->count].message[MAX_ERROR_MSG - 1] = '\0';
    list->errors[list->count].line = line;
    list->errors[list->count].column = column;
    list->errors[list->count].stage = stage;
    list->count++;
}

void error_list_write_log(const ErrorList *list)
{
    FILE *log = fopen("errors.log", "w");
    if (!log)
        return;

    if (list->count == 0)
    {
        fprintf(log, "Compilation completed successfully with no errors.\n");
    }
    else
    {
        static const char *stage_names[] = {
            "Lexer", "Parser", "Semantic", "Codegen", "General"};
        for (int i = 0; i < list->count; i++)
        {
            fprintf(log, "[%s] [line %d, col %d] %s\n",
                    stage_names[list->errors[i].stage],
                    list->errors[i].line,
                    list->errors[i].column,
                    list->errors[i].message);
        }
    }
    fclose(log);
}

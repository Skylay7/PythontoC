/* indent_stack.h */

#ifndef INDENT_STACK_H
#define INDENT_STACK_H

#define MAX_INDENT_DEPTH 128

typedef struct
{
    int values[MAX_INDENT_DEPTH];
    int top; // -1 means empty
} IndentStack;

void indent_stack_init(IndentStack *stack);
void indent_stack_push(IndentStack *stack, int value);
int  indent_stack_pop(IndentStack *stack);
int  indent_stack_peek(const IndentStack *stack);
int  indent_stack_is_empty(const IndentStack *stack);
int  indent_stack_size(const IndentStack *stack);

#endif

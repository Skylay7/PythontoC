/* indent_stack.c */

#include "indent_stack.h"

void indent_stack_init(IndentStack *stack)
{
    stack->top = -1;
}

void indent_stack_push(IndentStack *stack, int value)
{
    if (stack->top >= MAX_INDENT_DEPTH - 1)
        return;
    stack->values[++stack->top] = value;
}

int indent_stack_pop(IndentStack *stack)
{
    if (stack->top < 0)
        return -1;
    return stack->values[stack->top--];
}

int indent_stack_peek(const IndentStack *stack)
{
    if (stack->top < 0)
        return 0; // empty = indent level 0
    return stack->values[stack->top];
}

int indent_stack_is_empty(const IndentStack *stack)
{
    return stack->top < 0;
}

int indent_stack_size(const IndentStack *stack)
{
    return stack->top + 1;
}

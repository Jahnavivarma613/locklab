#include "stack.h"
#include <stdint.h>
#include <stdlib.h>

bool stack_init(stack_t *stack, size_t capacity)
{
    if (stack == NULL || capacity == 0 ||
        capacity > SIZE_MAX / sizeof(int))
        return false;

    stack->data = malloc(capacity * sizeof(int));
    if (stack->data == NULL) {
        stack->capacity = 0;
        stack->top = 0;
        return false;
    }

    stack->capacity = capacity;
    stack->top = 0;
    return true;
}

void stack_destroy(stack_t *stack)
{
    if (stack == NULL)
        return;

    free(stack->data);
    stack->data = NULL;
    stack->capacity = 0;
    stack->top = 0;
}

bool stack_is_empty(const stack_t *stack)
{
    return stack == NULL || stack->top == 0;
}

bool stack_is_full(const stack_t *stack)
{
    return stack != NULL && stack->top == stack->capacity;
}

bool stack_push(stack_t *stack, int value)
{
    if (stack == NULL || stack->data == NULL ||
        stack_is_full(stack))
        return false;

    stack->data[stack->top++] = value;
    return true;
}

bool stack_pop(stack_t *stack, int *value)
{
    if (stack == NULL || stack->data == NULL ||
        value == NULL || stack_is_empty(stack))
        return false;

    *value = stack->data[--stack->top];
    return true;
}

bool stack_peek(const stack_t *stack, int *value)
{
    if (stack == NULL || stack->data == NULL ||
        value == NULL || stack_is_empty(stack))
        return false;

    *value = stack->data[stack->top - 1];
    return true;
}

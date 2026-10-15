#ifndef STACK_H
#define STACK_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    int *data;
    size_t capacity;
    size_t top;
} stack_t;

bool stack_init(stack_t *stack, size_t capacity);
void stack_destroy(stack_t *stack);
bool stack_push(stack_t *stack, int value);
bool stack_pop(stack_t *stack, int *value);
bool stack_peek(const stack_t *stack, int *value);
bool stack_is_empty(const stack_t *stack);
bool stack_is_full(const stack_t *stack);

#endif

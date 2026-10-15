#include "treiber_stack.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    treiber_stack_t stack;
    size_t slot;
    task_t input = {0};
    task_t output = {0};

    if (!ts_init(&stack) ||
        !ts_register_thread(&stack, &slot)) {
        fprintf(stderr, "Stack initialization failed\n");
        return 1;
    }

    input.id = 42;
    snprintf(input.data, sizeof(input.data), "LockLab test task");

    if (!ts_push(&stack, &input)) {
        fprintf(stderr, "Push failed\n");
        ts_unregister_thread(&stack, slot);
        ts_destroy(&stack);
        return 1;
    }

    if (!ts_pop(&stack, slot, &output)) {
        fprintf(stderr, "Pop failed\n");
        ts_unregister_thread(&stack, slot);
        ts_destroy(&stack);
        return 1;
    }

    if (output.id != input.id ||
        strcmp(output.data, input.data) != 0) {
        fprintf(stderr, "FAIL: task data mismatch\n");
        ts_unregister_thread(&stack, slot);
        ts_destroy(&stack);
        return 1;
    }

    printf("PASS: pushed and popped task ID %" PRIu64 "\n",
           output.id);

    if (ts_pop(&stack, slot, &output)) {
        fprintf(stderr, "FAIL: expected empty stack\n");
        ts_unregister_thread(&stack, slot);
        ts_destroy(&stack);
        return 1;
    }

    printf("PASS: empty stack detected\n");

    ts_unregister_thread(&stack, slot);
    ts_destroy(&stack);
    return 0;
}

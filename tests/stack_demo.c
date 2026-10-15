
#include <stdio.h>
#include "stack.h"

int main(void)
{
    stack_t stack;
    int value, choice;
    size_t capacity;

    printf("Enter stack capacity: ");
    if (scanf("%zu", &capacity) != 1 || capacity == 0) {
        printf("Invalid capacity!\n");
        return 1;
    }

    if (!stack_init(&stack, capacity)) {
        printf("Stack initialization failed!\n");
        return 1;
    }

    do {
        printf("\n--- STACK MENU ---\n");
        printf("1. Push\n");
        printf("2. Pop\n");
        printf("3. Peek\n");
        printf("4. Display stack\n");
        printf("5. Exit\n");
        printf("Enter your choice: ");

        if (scanf("%d", &choice) != 1) {
            printf("Invalid input!\n");
            break;
        }

        switch (choice) {
        case 1:
            printf("Enter value to push: ");
            if (scanf("%d", &value) != 1) {
                printf("Invalid value!\n");
                stack_destroy(&stack);
                return 1;
            }

            if (stack_push(&stack, value))
                printf("%d pushed successfully.\n", value);
            else
                printf("Stack is full!\n");
            break;

        case 2:
            if (stack_pop(&stack, &value))
                printf("Popped value: %d\n", value);
            else
                printf("Stack is empty!\n");
            break;

        case 3:
            if (stack_peek(&stack, &value))
                printf("Top element: %d\n", value);
            else
                printf("Stack is empty!\n");
            break;

        case 4:
            if (stack_is_empty(&stack)) {
                printf("Stack is empty!\n");
            } else {
                printf("Stack (top to bottom):\n");
                for (size_t i = stack.top; i > 0; i--)
                    printf("%d\n", stack.data[i - 1]);
            }
            break;

        case 5:
            printf("Exiting stack program.\n");
            break;

        default:
            printf("Invalid choice! Try again.\n");
        }
    } while (choice != 5);

    stack_destroy(&stack);
    return 0;
}

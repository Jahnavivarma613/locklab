#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#define TASK_DATA_SIZE 64

typedef struct {
    uint64_t id;
    char data[TASK_DATA_SIZE];
} task_t;

#endif

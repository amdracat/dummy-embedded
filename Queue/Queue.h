#ifndef QUEUE_H
#define QUEUE_H

#include <stdbool.h>
#include <stdint.h>

#define QUEUE_SIZE 8

typedef struct {
    int data[QUEUE_SIZE];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
} Queue;

void Queue_Init(Queue *q);
bool Queue_IsEmpty(const Queue *q);
bool Queue_IsFull(const Queue *q);
bool Queue_Enqueue(Queue *q, int value);
bool Queue_Dequeue(Queue *q, int *out);

#endif

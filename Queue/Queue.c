#include "Queue.h"

void Queue_Init(Queue *q)
{
    q->head = 0;
    q->tail = 0;
    q->count = 0;
}

bool Queue_IsEmpty(const Queue *q)
{
    return q->count == 0;
}

bool Queue_IsFull(const Queue *q)
{
    return q->count == QUEUE_SIZE;
}

bool Queue_Enqueue(Queue *q, int value)
{
    if (Queue_IsFull(q)) {
        return false;
    }
    q->data[q->tail] = value;
    q->tail = (q->tail + 1) % QUEUE_SIZE;
    q->count++;
    return true;
}

bool Queue_Dequeue(Queue *q, int *out)
{
    if (Queue_IsEmpty(q)) {
        return false;
    }
    *out = q->data[q->head];
    q->head = (q->head + 1) % QUEUE_SIZE;
    q->count--;
    return true;
}

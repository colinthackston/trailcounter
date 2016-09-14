#include "queue.h"

// We say that a queue is...
// - full when head is equal to tail
// - empty when head exactly one ahead of tail

void init_queue(queue_t *q) {
  q->head = 1;
  q->tail = 0;
  int i;
  for (i = 0; i < QUEUE_SIZE; i++) {
    q->buffer[i] = 0;
  }
}

int enqueue (queue_t *q, int data) {
  if (q->head == q->tail) {
    // fail
    return 0;
  }
  q->buffer[q->head] = data;
  q->head = (q->head + 1) % QUEUE_SIZE;
  return 1;
}

int dequeue (queue_t *q) {
  if (queue_empty(q)) {
    // fail
    return 0;
  }

  int nextTailIdx = (q->tail + 1) % QUEUE_SIZE;
  int result = q->buffer[nextTailIdx];
  q->tail = nextTailIdx;

  return result;
}

int queue_empty(queue_t *q) {
  int incrementedTail = (q->tail + 1) % QUEUE_SIZE;
  return incrementedTail == q->head;
}


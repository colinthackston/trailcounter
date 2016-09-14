/*Filename:  queue.h*
Contains the prototypes of the queue.c and the queue_t class to be used for the
read and write steams for the Uart. 
*Part of: C335 Lab Assignment 11 *
*Created by: Mike Hyde / Seth Kirby *
*Created on: April 11, 2014 *
*Last Modified by: Seth Kirby *
*Last Modified on:  April 11, 2014* */

/****************************************************************************\
******************************************************************************
** /*Filename:  queue.h*
Contains the prototypes of the queue.c and the queue_t class to be used for the
read and write steams for the Uart. 
*Part of: C335 Lab Assignment 11 *
*Created by: Mike Hyde / Seth Kirby *
*Created on: April 11, 2014 *
*Last Modified by: Seth Kirby *
*Last Modified on:  April 11, 2014*                                   **
******************************************************************************
\****************************************************************************/

#define QUEUE_SIZE 32

typedef struct queue {
  int head;
  int tail;
  int buffer[QUEUE_SIZE];
} queue_t;

void init_queue(queue_t *);
int enqueue(queue_t *, int);
int dequeue(queue_t *);            
int queue_empty(queue_t *buf);
int peek(queue_t *);
int queue_full(queue_t *);            

static queue_t rxbuf;
static queue_t txbuf;

/* queue.h ends here */

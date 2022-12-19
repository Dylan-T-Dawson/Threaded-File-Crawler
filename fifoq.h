//The header file for a thread-safe FIFO Queue implementation
#ifndef FIFOQ_H
#define FIFOQ_H

#define DEBUG(s) printf(s); fflush(stdout)

typedef struct fq_item {
  struct fq_item *fqi_next;  // next item in queue
  void *fqi_payload;         // what will be returned to user when dequeued
} fqi_t;

typedef struct fifoq {
  fqi_t *fq_first; //points to first item in queue.
  fqi_t *fq_last; //points to last item in queue.
  pthread_mutex_t fq_lock; //Lock for the queue.
  pthread_cond_t fq_mtcond; //Conditional variable for blocking a thread while the queue is empty.
  pthread_cond_t fq_fincond; //Conditional variables for when all threads operating on the queue are blocked and the queue.
  int fq_nwaiting; //Number of blocked threads.
  int fq_limit; //Limit of blocked threads before the queue can finish.
} fifoq_t;

void fq_init(fifoq_t *q, int limit);  // initialize and configure
void fq_add(fifoq_t *q, void *item); // put something in the queue
void *fq_get(fifoq_t *q);            // get the first thing
void fq_finish(fifoq_t *q);  // Blocks until the queue is empty and all threads are waiting.
#endif

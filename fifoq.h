#ifndef FIFOQ_H
#define FIFOQ_H

#define DEBUG(s) printf(s); fflush(stdout)

typedef struct fq_item {
  struct fq_item *fqi_next;  // next item in queue
  void *fqi_payload;         // what will be returned to user when dequeued
} fqi_t;

typedef struct fifoq {
  fqi_t *fq_first;
  fqi_t *fq_last;
  pthread_mutex_t fq_lock;
  pthread_cond_t fq_mtcond;
  pthread_cond_t fq_fincond;
  int fq_nwaiting;
  int fq_limit;
} fifoq_t;

void fq_init(fifoq_t *q, int limit);  // initialize and configure
void fq_add(fifoq_t *q, void *item); // put something in the queue
void *fq_get(fifoq_t *q);            // get the first thing
void fq_finish(fifoq_t *q);  // how many are blocked on empty queue?
#endif

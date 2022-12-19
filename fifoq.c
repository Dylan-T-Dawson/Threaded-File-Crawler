//Implementation of a thread safe FIFO Queue.
#include <stdio.h>
#include <pthread.h>
#include <malloc.h>
#include "fifoq.h"
#include <stdlib.h>


/* Invariant:  fq_first == NULL <=> fq_last == NULL */
void fq_init(fifoq_t *q, int limit) {
  
  //Initializes mutex and condition variables as well as establishing the invariant.
  if (pthread_mutex_init(&(q->fq_lock),NULL)) {
        fprintf(stderr,"mutex init");
        exit(9);
  }
  if (pthread_cond_init(&(q->fq_mtcond), NULL)) {
        fprintf(stderr,"cond init");
        exit(9);
  }
  if (pthread_cond_init(&(q->fq_fincond), NULL)) {
        fprintf(stderr,"cond init");
        exit(9);
  }

  //The lock below is somewhat unnecessary, as fq_init should only be called once. However, it is included for safety in the case that init is called twice on the same queue.
  pthread_mutex_lock(&(q->fq_lock));
  q->fq_first = q->fq_last = NULL;
  q->fq_nwaiting = 0;
  q->fq_limit = limit;
  pthread_mutex_unlock(&(q->fq_lock));
}

void fq_add(fifoq_t *q, void *item) {
  /* create an item to hold the value */
  fqi_t *np = malloc(sizeof(fqi_t));
  np->fqi_payload = item;
  np->fqi_next = NULL;

  /* critical section */
  pthread_mutex_lock(&(q->fq_lock));
  if (q->fq_last == NULL) { // was empty
    np->fqi_next = NULL;
    q->fq_first = np;
    q->fq_last = np;
    if (q->fq_nwaiting > 0) {
      /* Wake up a waiting thread */
   	  pthread_cond_signal(&(q->fq_mtcond));
    }

  } else { // something in queue
    q->fq_last->fqi_next = np;
    q->fq_last = np;
  }
  pthread_mutex_unlock(&(q->fq_lock));
  /* end critical section */
}

// remove and return the head of the queue, if it exists. Else, block.
void *fq_get(fifoq_t *q) {
  /* critical section */
  pthread_mutex_lock(&(q->fq_lock));
  while (q->fq_first == NULL) { // queue is empty => block
    q->fq_nwaiting += 1; // adjust number waiting
    if (q->fq_nwaiting == q->fq_limit) {
    /*Signals that the limit of waiting threads is reached */
	    pthread_cond_signal(&(q->fq_fincond));
    }
    /* Blocks until another thread signals that the queue is nonempty */
    while(q->fq_first == NULL){ 
      pthread_cond_wait(&(q->fq_mtcond), &(q->fq_lock));
    }
    q->fq_nwaiting -= 1; // not waiting now
  }
  /* q->fq_first != NULL */
  fqi_t *p = q->fq_first;
  q->fq_first = p->fqi_next;
  /* if queue is now empty, set fq_last = NULL */
  if (p->fqi_next == NULL)
    q->fq_last = NULL;
  void *item = p->fqi_payload;
  /* end critical section */
  pthread_mutex_unlock(&(q->fq_lock));
  free(p);
  return item;
}

/*
 * Block until the number of waiting threads is equal to the limit,
 * and the queue is empty.
 */
void fq_finish(fifoq_t *q) {
  /* critical section */
  pthread_mutex_lock(&(q->fq_lock));
  while (q->fq_first != NULL ||        // nonempty
	 q->fq_nwaiting < q->fq_limit) {
    /* Blocks until another thread signals that conditions have changed */
	  pthread_cond_wait(&(q->fq_fincond), &(q->fq_lock));
  }
  /* q->fq_first == NULL && q->fq_nwaiting >= q->fq_limit */
  /* end critical section */
  pthread_mutex_unlock(&(q->fq_lock));
  return 1;
}


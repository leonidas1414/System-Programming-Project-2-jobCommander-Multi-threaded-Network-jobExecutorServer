#ifndef PROD_CONS_H
#define PROD_CONS_H

#include "queue.h"


typedef struct synced_queue_t {
    Job ** data;
    int start;
    int end;
    int count;
    int POOL_SIZE;
} synced_queue_t;


synced_queue_t * synced_queue_initialize(int POOL_SIZE);

void synced_queued_destroy(synced_queue_t * queue);

void synced_queue_insert(synced_queue_t * queue, char * job_id, char * command, int socket);

Job * synced_queue_remove_first(synced_queue_t * queue);

void synced_queue_send_jobs(synced_queue_t * queue, int fd);

void synced_queue_cancel_jobs(synced_queue_t * queue, int fd);

int synced_queue_remove(synced_queue_t * queue, char * job_id);


#endif /* PROD_CONS_H */
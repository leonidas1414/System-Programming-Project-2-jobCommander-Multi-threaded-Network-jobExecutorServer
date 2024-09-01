#include <stdio.h>   // from www.mario-konrad.ch
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../include/prod_cons.h"
#include "../include/protocol_executor.h"
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond_nonempty = PTHREAD_COND_INITIALIZER;
static pthread_cond_t cond_nonfull = PTHREAD_COND_INITIALIZER;

synced_queue_t * synced_queue_initialize(int POOL_SIZE) {
    synced_queue_t * queue = malloc(sizeof (struct synced_queue_t));

    queue->start = 0;
    queue->end = -1;
    queue->count = 0;
    queue->POOL_SIZE = POOL_SIZE;
    queue->data = malloc(sizeof (Job*) * POOL_SIZE);

    for (int i = 0; i < POOL_SIZE; i++) {
        queue->data[i] = NULL;
    }
    return queue;
}

void synced_queued_destroy(synced_queue_t * queue) {
    free(queue->data);
    free(queue);
}

void synced_queue_insert(synced_queue_t * queue, char * job_id, char * command, int socket) {
    Job * job = (Job *) calloc(1, sizeof (Job));
    strcpy(job->job_id, job_id);
    strcpy(job->command, command);
    job->socket = socket;
    job->pid = -1;

    pthread_mutex_lock(&mtx);
    while (queue->count >= queue->POOL_SIZE) {
        pthread_cond_wait(&cond_nonfull, &mtx);
    }
    queue->end = (queue->end + 1) % queue->POOL_SIZE;
    queue->data[queue->end] = job;
    queue->count++;
    pthread_mutex_unlock(&mtx);

    pthread_cond_signal(&cond_nonempty);
}

Job * synced_queue_remove_first(synced_queue_t * queue) {
    Job * job = 0;

    pthread_mutex_lock(&mtx);
    while (queue->count <= 0) {
        pthread_cond_wait(&cond_nonempty, &mtx);
    }
    job = queue->data[queue->start];
    queue->data[queue->start] = NULL;
    queue->start = (queue->start + 1) % queue->POOL_SIZE;
    queue->count--;
    pthread_mutex_unlock(&mtx);

    pthread_cond_signal(&cond_nonfull);

    return job;
}

void synced_queue_send_jobs(synced_queue_t * queue, int fd) {
    pthread_mutex_lock(&mtx);

    int index = queue->start;
    int position = 0;

    int size = 4000;
    char * buffer = calloc(size, sizeof (char));
    int used_length = 0;

    while (position < queue->count) {
        if (queue->data[index] != NULL) {
            int node_length = 3 + strlen(queue->data[index]->command) + strlen(queue->data[index]->job_id) + 120;

            if (size - used_length > node_length) {
                char * buffer2 = calloc(node_length, sizeof (char));
                sprintf(buffer2, "[%3d] %80s : %10s \n", position, queue->data[index]->command, queue->data[index]->job_id);

                strcat(buffer, buffer2);

                free(buffer2);

                used_length = used_length + node_length;
            } else {
                while (size - used_length <= node_length) {
                    size = size * 2;
                }

                buffer = realloc(buffer, size);

                char * buffer2 = calloc(node_length, sizeof (char));
                sprintf(buffer2, "[%3d] %80s : %10s \n", position, queue->data[index]->command, queue->data[index]->job_id);

                strcat(buffer, buffer2);

                free(buffer2);

                used_length = used_length + node_length;
            }
        }

        position++;

        index = index + 1;
        index = index % queue->POOL_SIZE;
    }

    if (used_length > 0) {
        send_message(fd, buffer);
    } else {
        send_message(fd, "no data");
    }

    free(buffer);

    pthread_mutex_unlock(&mtx);
}

void synced_queue_cancel_jobs(synced_queue_t * queue, int fd_out) {
    pthread_mutex_lock(&mtx);

    int index = queue->start;
    int position = 0;

    while (position < queue->count) {
        if (queue->data[index] != NULL) {
            printf("Closing job at position index: %d \n", index);
            
            int fd = queue->data[index]->socket;

            char * buffer = calloc(500, sizeof (char));
            strcpy(buffer, "SERVER TERMINATED BEFORE EXECUTION");
            
            printf("Sending SERVER TERMINATED to fd: %d ... \n", fd);
            
            send_message(fd, buffer);
            
            queue->data[index] = NULL;
            
            close(fd);            
        }

        position++;

        index = index + 1;
        index = index % queue->POOL_SIZE;
    }

    queue->count = 0;

    pthread_mutex_unlock(&mtx);
    
    send_message(fd_out, "jobExecutorServer terminated");
}

int synced_queue_remove(synced_queue_t * queue, char * job_id) {
    pthread_mutex_lock(&mtx);
    
    int index = queue->start;
    int position = 0;

    while (position < queue->count) {
        if (queue->data[index] != NULL && strcmp(queue->data[index]->job_id, job_id) == 0) {
            printf("Closing job at position index: %d \n", index);
            
            int fd = queue->data[index]->socket;

            char * buffer = calloc(500, sizeof (char));
            strcpy(buffer, "JOB CANCELLED BY THE USER");
            
            printf("Sending SERVER TERMINATED to fd: %d ... \n", fd);
            
            send_message(fd, buffer);
            
            strcpy(queue->data[index]->command, "#");
            
            close(fd);  
            
            break;
        }

        position++;

        index = index + 1;
        index = index % queue->POOL_SIZE;
    }
    
    int nodes_deleted = 0;
    
    if (position < queue->count) {
        nodes_deleted++;
    }
    
    pthread_mutex_unlock(&mtx);
    
    return nodes_deleted;
}
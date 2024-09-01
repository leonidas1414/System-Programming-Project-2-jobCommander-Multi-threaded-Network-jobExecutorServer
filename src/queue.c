#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>

#include "../include/queue.h"
#include "../include/protocol_executor.h"

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

Queue * queue_initialize() {
    pthread_mutex_lock(&mutex);
    
    Queue * q = (Queue *) malloc(sizeof (Queue));
    q->head = NULL;
    
    pthread_mutex_unlock(&mutex);
    
    return q;
}

void queue_destroy(Queue * q) {
    if (q == NULL) {
        return;
    }
    
    pthread_mutex_lock(&mutex);

    QueueNode * qn = q->head;

    while (qn != NULL) {
        QueueNode * next = qn->next;
        free(qn->job);
        free(qn);
        qn = next;
    }

    free(q);
    
    pthread_mutex_unlock(&mutex);
}

int queue_insert(Queue * q, char * job_id, char * command) {
    if (q == NULL) {
        return -1;
    }
    
    pthread_mutex_lock(&mutex);

    if (q->head == NULL) {
        q->head = (QueueNode*) calloc(1, sizeof (QueueNode));
        Job * job = (Job *) calloc(1, sizeof (Job));
        strcpy(job->job_id, job_id);
        strcpy(job->command, command);
        job->pid = -1;
        q->head->job = job;

        pthread_mutex_unlock(&mutex);
        
        return 0;
    } else {
        QueueNode * qn = q->head;

        int position = 1;

        while (qn->next) {
            if (qn->job != NULL) {
                position++;
            }
            qn = qn->next;
        }

        qn->next = (QueueNode*) calloc(1, sizeof (QueueNode));
        Job * job = (Job *) calloc(1, sizeof (Job));
        strcpy(job->job_id, job_id);
        strcpy(job->command, command);
        job->pid = -1;
        qn->next->job = job;

        pthread_mutex_unlock(&mutex);
        
        return position;
    }
}

int queue_insert_with_pid(Queue * q, char * job_id, char * command, pid_t child_pid) {
    if (q == NULL) {
        return -1;
    }

    pthread_mutex_lock(&mutex);
    
    if (q->head == NULL) {
        q->head = (QueueNode*) calloc(1, sizeof (QueueNode));
        Job * job = (Job *) calloc(1, sizeof (Job));
        strcpy(job->job_id, job_id);
        strcpy(job->command, command);
        job->pid = child_pid;
        q->head->job = job;

        pthread_mutex_unlock(&mutex);
        
        return 0;
    } else {
        QueueNode * qn = q->head;

        int position = 1;

        while (qn->next) {
            if (qn->job != NULL) {
                position++;
            }
            qn = qn->next;
        }

        qn->next = (QueueNode*) calloc(1, sizeof (QueueNode));
        Job * job = (Job *) calloc(1, sizeof (Job));
        strcpy(job->job_id, job_id);
        strcpy(job->command, command);
        job->pid = child_pid;
        qn->next->job = job;

        pthread_mutex_unlock(&mutex);
        
        return position;
    }
}

Job * queue_remove(Queue * q) {
    pthread_mutex_lock(&mutex);
    
    if (q->head == NULL) {
        pthread_mutex_unlock(&mutex);
        
        return NULL;
    }

    
    while (q->head != NULL && q->head->job == NULL) {
        QueueNode * qn = q->head;

        q->head = q->head->next;

        free(qn);
    }

    if (q->head == NULL) {
        pthread_mutex_unlock(&mutex);
        return NULL;
    }

    Job * job = q->head->job;

    QueueNode * qn = q->head;

    q->head = q->head->next;

    free(qn);

    pthread_mutex_unlock(&mutex);
    
    return job;
}

Job * queue_delete_with_pid(Queue * q, pid_t pid) {
    pthread_mutex_lock(&mutex);
    
    QueueNode * qn = q->head;

    while (qn != NULL) {
        if (qn->job != NULL) {
            if (qn->job->pid == pid) {
                free(qn->job);
                qn->job = NULL;
                
                pthread_mutex_unlock(&mutex);
                
                return NULL;
            }
        }

        qn = qn->next;
    }
    
    pthread_mutex_unlock(&mutex);

    return 0;
}

int queue_delete(Queue * q, char * job_id) {
    pthread_mutex_lock(&mutex);
    
    QueueNode * qn = q->head;

    while (qn != NULL) {
        if (qn->job != NULL) {
            if (strcmp(qn->job->job_id, job_id) == 0) {
                if (qn->job->pid > 0) {
                    kill(qn->job->pid, SIGINT);
                }

                free(qn->job);
                qn->job = NULL;
                
                pthread_mutex_unlock(&mutex);
                
                return 1;
            }
        }

        qn = qn->next;
    }

    pthread_mutex_unlock(&mutex);
    
    return 0;

}

void queue_send_jobs(Queue * q, int fd) {
    pthread_mutex_lock(&mutex);
    
    QueueNode * qn = q->head;

    int position = 0;

    int size = 4000;
    char * buffer = calloc(size, sizeof (char));
    int used_length = 0;

    while (qn != NULL) {
        if (qn->job != NULL) {
            int node_length = 3 + strlen(qn->job->command) + strlen(qn->job->job_id) + 120;

            if (size - used_length > node_length) {
                char * buffer2 = calloc(node_length, sizeof (char));
                sprintf(buffer2, "[%3d] %80s : %10s \n", position, qn->job->command, qn->job->job_id);

                strcat(buffer, buffer2);

                free(buffer2);

                used_length = used_length + node_length;
            } else {
                while (size - used_length <= node_length) {
                    size = size * 2;
                }

                buffer = realloc(buffer, size);

                char * buffer2 = calloc(node_length, sizeof (char));
                sprintf(buffer2, "[%3d] %80s : %10s \n", position, qn->job->command, qn->job->job_id);

                strcat(buffer, buffer2);

                free(buffer2);

                used_length = used_length + node_length;
            }

            position++;
        }

        qn = qn->next;
    }

    if (used_length > 0) {
        send_message(fd, buffer);
    } else {
        send_message(fd, "no data");
    }

    free(buffer);
    
    pthread_mutex_unlock(&mutex);
}

void queue_print(Queue * q) {
    pthread_mutex_lock(&mutex);
    
    QueueNode * qn = q->head;

    int position = 0;

    while (qn != NULL) {
        if (qn->job != NULL) {
            printf("[%3d] %30s : %30s \n", position, qn->job->job_id, qn->job->command);
            position++;
        }

        qn = qn->next;
    }
    
    pthread_mutex_unlock(&mutex);
}

int queue_size(Queue * q) {
    pthread_mutex_lock(&mutex);
    
    QueueNode * qn = q->head;

    int c = 0;

    while (qn != NULL) {
        if (qn->job != NULL) {
            c++;
        }

        qn = qn->next;
    }

    pthread_mutex_unlock(&mutex);
    
    return c;
}
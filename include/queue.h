
#ifndef QUEUE_H
#define QUEUE_H

typedef struct Job {
    char job_id[100];
    char command[4096];
    pid_t pid;
    int socket;    
} Job;

typedef struct QueueNode {
    Job * job;
    struct QueueNode * next;
} QueueNode;

typedef struct Queue {
    QueueNode * head;
} Queue;


Queue * queue_initialize();

void queue_destroy(Queue * q);

int queue_insert(Queue * q, char * job_id, char * command);

int queue_insert_with_pid(Queue * q, char * job_id, char * command, pid_t pid);

Job * queue_remove(Queue * q);

int queue_delete(Queue * q, char * job_id);

Job * queue_delete_with_pid(Queue * q, pid_t pid);


void queue_send_jobs(Queue * q, int fd);

void queue_print(Queue * q);


int queue_size(Queue * q);

#endif /* QUEUE_H */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

#include <sys/socket.h>      /* sockets */
#include <netinet/in.h>      /* internet sockets */
#include <netdb.h>          /* gethostbyaddr */
#include <ctype.h>          /* toupper */
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <pthread.h>
#include <linux/limits.h>          /* signal */

#include "../include/queue.h"
#include "../include/protocol_executor.h"
#include "../include/server_executor.h"
#include "../include/prod_cons.h"

#define PIPE_COMMANDER_TO_SERVER "commander_to_server.pipe"
#define PIPE_SERVER_TO_COMMANDER "server_to_commander.pipe"

bool ctrl_c_pressed = false;

static Queue * running;

static struct {
    int fd_listening_socket;
    pthread_mutex_t SOCKET_LOCK;
} socket_synchronization = {0, PTHREAD_MUTEX_INITIALIZER};

static struct {
    int concurrency;
    int forks;
    pthread_mutex_t CONCURRENCY_LOCK;
    pthread_cond_t CONCURRENCY_CONDITION;
} concurrency_synchronization = {4, 0, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER};

static struct {
    int counter;
    pthread_mutex_t ID_LOCK;
} id_synchronization = {0, PTHREAD_MUTEX_INITIALIZER};


static int _portNum = 0;
static int _bufferSize = 0;
static int _threadPoolSize = 0;
static pthread_t main_thread_pid;

static sigset_t mask_unblock;
static sigset_t mask_block;

static pthread_t * worker_thread_ids;

static synced_queue_t * synced_queue;

void application_shutdown_handler(int signum) {
    printf("Received signal: %d\n", signum);
    ctrl_c_pressed = true;
}

void child_exited_shutdown_handler(int signum) {
    printf("Received signal: %d\n", signum);

    int status;

    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            printf("Child exited - Exit status from %u was %d\n", pid, exit_status);

            queue_delete_with_pid(running, pid);
        }
    }
}

void server_start_up(int portNum, int bufferSize, int threadPoolSize) {
    printf("Starting server ... port: %d, buffer size: %d, thread pool size: %d  \n", portNum, bufferSize, threadPoolSize);

    _portNum = portNum;
    _bufferSize = bufferSize;
    _threadPoolSize = threadPoolSize;

    sigemptyset(&mask_unblock);

    sigemptyset(&mask_block);
    sigaddset(&mask_block, SIGCHLD);

    running = queue_initialize();

    main_thread_pid = pthread_self();

    // 
    // Socket code
    //

    struct sockaddr_in server;

    struct sockaddr *serverptr = (struct sockaddr *) &server;
    int rc;

    printf("Creating listening socket ... \n");

    if ((socket_synchronization.fd_listening_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(socket_synchronization.fd_listening_socket);
    }
    server.sin_family = AF_INET; /* Internet domain */
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(portNum);

    if ((rc = bind(socket_synchronization.fd_listening_socket, serverptr, sizeof (server))) < 0) {
        perror("bind");
        exit(rc);
    }

    if ((rc = listen(socket_synchronization.fd_listening_socket, 200)) < 0) {
        perror("listen");
        exit(rc);
    }


    struct sigaction application_shutdown_action;
    application_shutdown_action.sa_handler = application_shutdown_handler;
    application_shutdown_action.sa_flags = 0;

    sigemptyset(&application_shutdown_action.sa_mask);

    if (sigaction(SIGINT, &application_shutdown_action, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    //
    // Create producer consumer queue
    //

    synced_queue = synced_queue_initialize(bufferSize);

    //
    // Create worker thread pool
    //   
    worker_thread_ids = malloc(sizeof (pthread_t) * threadPoolSize);

    for (int i = 0, err = 0; i < _threadPoolSize; i++) {
        pthread_t thr;

        int * copy = malloc(sizeof (int));
        *copy = i + 1;

        if ((err = pthread_create(&thr, NULL, server_worker_thread_main, copy)) < 0) { /* New thread */
            perror("pthread_create");
            exit(1);
        }

        worker_thread_ids[i] = thr;
    }

    printf("Server setup complete \n");
}

void server_exec(char ** argv) {
    char path[PATH_MAX] = "";

    int fd;

    snprintf(path, PATH_MAX, "%d.output", getpid());

    fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0666);

    if (fd != -1) {
        dup2(fd, 1);
    }

    exit(execvp(argv[0], argv));
}

pid_t server_fork(Job * job) {
    pid_t pid = fork();

    if (pid == 0) {
        char * cmd = strdup(job->command);

        int counter = 0;
        char * saveptr = NULL;
        strtok_r(cmd, " ", &saveptr);
        counter++;

        while (strtok_r(NULL, " ", &saveptr) != NULL) {
            counter++;
        }
        free(cmd);

        cmd = strdup(job->command);

        char ** argv = malloc(sizeof (char*)*(1 + counter));

        argv[0] = strtok_r(cmd, " ", &saveptr);

        for (int i = 1; i < counter; i++) {
            argv[i] = strtok_r(NULL, " ", &saveptr);
        }

        argv[counter] = NULL;

        server_exec(argv);

        free(argv);
    }

    if (pid < 0) {
        perror("fork - exec failed ");
    }

    return pid;
}

void server_service() {
    pthread_t thr;
    int err;

    while (!ctrl_c_pressed) {
        printf("Check starting jobs .... \n");

        //        server_check_start_jobs();

        printf("Waiting for a job .... \n");

        int newsock;

        struct sockaddr_in client;
        struct sockaddr *clientptr = (struct sockaddr *) &client;
        socklen_t clientlen = sizeof (client);

        pthread_mutex_lock(&socket_synchronization.SOCKET_LOCK);
        printf(" === Main thread - Waiting for connection at fd:%d === \n", socket_synchronization.fd_listening_socket);

        int copy_listening_socket = socket_synchronization.fd_listening_socket;

        pthread_mutex_unlock(&socket_synchronization.SOCKET_LOCK);


        if ((newsock = accept(copy_listening_socket, clientptr, &clientlen)) < 0) {
            if (errno == EINTR) {
                printf(" === Main thread interrupted (SIGINT) === ... \n");
                break;
            }

            printf(" === Main thread interrupted (NOT SIGINT) === ... \n");
            break;
        }

        printf(" === Main thread - Accepted connection === \n");

        int * copy = (int *) malloc(sizeof (int));
        *copy = newsock;

        if ((err = pthread_create(&thr, NULL, server_controller_thread_main, (void*) copy)) < 0) { /* New thread */
            perror("pthread_create");
            exit(err);
        }
        
        pthread_detach(thr);
    }
}

void server_stop() {
    printf("Stopping server ... \n");

    printf("Shutting down communication with clients at fd:%d  ... \n", socket_synchronization.fd_listening_socket);

    shutdown(SHUT_RDWR, socket_synchronization.fd_listening_socket);

    close(socket_synchronization.fd_listening_socket);

    //
    // Waiting for worker threads to exit
    // 
    for (int i = 0; i < _threadPoolSize; i++) {
        synced_queue_insert(synced_queue, "-", "-", 0);
    }

    for (int i = 0; i < _threadPoolSize; i++) {
        pthread_t thr = worker_thread_ids[i];
        pthread_join(thr, 0);

        printf(" === Worker thread %d exited === \n", i);
    }

    queue_destroy(running);

    //
    // Destroy worker thread pool
    // 

    free(worker_thread_ids);

    //
    // Destroy thread pool size
    //

    synced_queued_destroy(synced_queue);

}

void * server_controller_thread_main(void * args) {
    int * copy = (int *) args;
    int newsock = *copy;
    int fd_in = newsock;
    int fd_out = newsock;
    free(copy);

    char * message = receive_message(fd_in);

    if (message == 0) {
        return 0;
    }

    printf("Message received: %s \n", message);

    UserCommand userCommand = parse_command(message);

    printf("Parsing test: [%s] [%s] [%d] \n", userCommand.cmd, userCommand.text, userCommand.value);

    char * job_id = 0;
    bool deleted_from_running = false;
    bool deleted_from_queued = false;

    //
    // Execution
    //

    if (strcmp(userCommand.cmd, "ISSUEJOB") == 0) {
        job_id = calloc(15, sizeof (char));

        pthread_mutex_lock(&id_synchronization.ID_LOCK);
        sprintf(job_id, "job_%d", ++id_synchronization.counter);
        pthread_mutex_unlock(&id_synchronization.ID_LOCK);

        printf(" ===> Queued: %s - %s \n", job_id, userCommand.text);
    }

    if (strcmp(userCommand.cmd, "STOP") == 0) {
        job_id = userCommand.text;

        int nodes_deleted;

        nodes_deleted = synced_queue_remove(synced_queue, job_id);

        printf("Deleted nodes from pending queue: %d \n", nodes_deleted);

        if (nodes_deleted > 0) {
            deleted_from_queued = true;
        }

        job_id = NULL;
    }

    if (strcmp(userCommand.cmd, "SET_CONCURRENCY") == 0) {
        printf("Concurrency set to %d \n", concurrency_synchronization.concurrency);

        char * m;

        pthread_mutex_lock(&concurrency_synchronization.CONCURRENCY_LOCK);

        if (userCommand.value < 1) {
            m = "INFO: concurrency out of range";
        } else if (userCommand.value < concurrency_synchronization.concurrency) {
            concurrency_synchronization.concurrency = userCommand.value;
            m = "INFO: concurrency updated successfully";
        } else if (userCommand.value == concurrency_synchronization.concurrency) {
            m = "INFO: concurrency not changed";
        } else {
            concurrency_synchronization.concurrency = userCommand.value;
            m = "INFO: concurrency updated successfully";
            //            server_check_start_jobs();
        }

        pthread_cond_broadcast(&concurrency_synchronization.CONCURRENCY_CONDITION);

        pthread_mutex_unlock(&concurrency_synchronization.CONCURRENCY_LOCK);

        puts(m);
    }

    if (strcmp(userCommand.cmd, "POLL_QUEUED") == 0) {
        printf("Sending queued jobs ... \n");

        synced_queue_send_jobs(synced_queue, fd_out);
    }

    //
    // Response
    //
    if (strcmp(userCommand.cmd, "EXIT") == 0) {
        printf("Exit activated ... \n");

        printf("Cleaning up queue ... \n");

        synced_queue_cancel_jobs(synced_queue, fd_out);

        printf("Clean up complete ... \n");

        ctrl_c_pressed = true;

        pthread_kill(main_thread_pid, SIGINT);

        printf("Controller thread exiting ... \n ");
    } else if (strcmp(userCommand.cmd, "STOP") == 0) {

        if (deleted_from_running) {
            send_message(fd_out, "job terminated");
        } else if (deleted_from_queued) {
            send_message(fd_out, "job removed");
        } else {
            send_message(fd_out, "job not found");
        }
    } else if (strcmp(userCommand.cmd, "SET_CONCURRENCY") == 0) {
        char * buffer = calloc(5000, sizeof (char));
        sprintf(buffer, "CONCURRENCY SET AT %d", userCommand.value);
        send_message(fd_out, buffer);
        free(buffer);
    } else if (strcmp(userCommand.cmd, "POLL_RUNNING") == 0) {
        // do nothing
    } else if (strcmp(userCommand.cmd, "POLL_QUEUED") == 0) {
        // do nothing
    } else if (strcmp(userCommand.cmd, "ISSUEJOB") == 0) {
        char * buffer = calloc(5000, sizeof (char));
        sprintf(buffer, "JOB (%s,%s) SUBMITTED", job_id, userCommand.text);
        send_message(fd_out, buffer);
        free(buffer);

        synced_queue_insert(synced_queue, job_id, userCommand.text, newsock);

    } else {
        send_message(fd_out, "200");
    }

    //    server_check_start_jobs();

    if (job_id) {
        free(job_id);
    }

    free(message);

    if (strcmp(userCommand.cmd, "ISSUEJOB") != 0) {
        close(newsock);
    }

    return 0;
}

void * server_worker_thread_main(void * args) {
    int * copy = (int*) args;
    int id = *copy;
    free(copy);

    printf(" === Worker thread %d started === \n", id);

    Job * job = NULL;

    while ((job = synced_queue_remove_first(synced_queue)) != NULL) {
        if (strcmp(job->job_id, "-") == 0) {
            break;
        }

        if (strcmp(job->job_id, "#") == 0) {
            free(job);
            continue;
        }

        pthread_mutex_lock(&concurrency_synchronization.CONCURRENCY_LOCK);
        while (concurrency_synchronization.forks >= concurrency_synchronization.concurrency) {
            pthread_cond_wait(&concurrency_synchronization.CONCURRENCY_CONDITION, &concurrency_synchronization.CONCURRENCY_LOCK);
        }

        concurrency_synchronization.forks++;
        pthread_mutex_unlock(&concurrency_synchronization.CONCURRENCY_LOCK);

        printf(" ===> Starting: %s - %s \n", job->job_id, job->command);

        pid_t child_pid = server_fork(job);

        if (child_pid < 0) {
            printf(" ===> Start failed: %s - %s \n", job->job_id, job->command);

            pthread_mutex_lock(&concurrency_synchronization.CONCURRENCY_LOCK);
            concurrency_synchronization.forks--;
            pthread_cond_broadcast(&concurrency_synchronization.CONCURRENCY_CONDITION);
            pthread_mutex_unlock(&concurrency_synchronization.CONCURRENCY_LOCK);

            send_message(job->socket, "job exec failed");
        } else {
            char path[PATH_MAX] = "";

            snprintf(path, PATH_MAX, "%d.output", child_pid);


            printf(" ===> Waiting: %s - %s \n", job->job_id, job->command);

            waitpid(child_pid, 0, 0);


            int hard_disk_fd = open(path, O_RDONLY);

            if (hard_disk_fd == -1) {
                char * output = "job complete - open failed (error)";

                send_message(job->socket, output);
            } else {
                char block[1024];

                while (1) {
                    int n = read(hard_disk_fd, block, sizeof (block));

                    if (n <= 0) {
                        break;
                    }

                    n = write_all(job->socket, block, n);

                    if (n <= 0) {
                        break;
                    }
                }

                close(hard_disk_fd);
                
                unlink(path);
            }

            pthread_mutex_lock(&concurrency_synchronization.CONCURRENCY_LOCK);
            concurrency_synchronization.forks--;
            pthread_cond_broadcast(&concurrency_synchronization.CONCURRENCY_CONDITION);
            pthread_mutex_unlock(&concurrency_synchronization.CONCURRENCY_LOCK);
        }

        if (job->socket > 0) {
            close(job->socket);
        }
        free(job);
    }

    if (job != NULL) {
        free(job);
    }

    return 0;
}
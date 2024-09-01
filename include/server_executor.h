#ifndef SERVER_H
#define SERVER_H

void server_start_up(int portNum, int bufferSize, int threadPoolSize);

void server_stop();

void server_service();

void server_check_start_jobs();

void * server_controller_thread_main(void * args);

void * server_worker_thread_main(void * args);
#endif /* SERVER_H */
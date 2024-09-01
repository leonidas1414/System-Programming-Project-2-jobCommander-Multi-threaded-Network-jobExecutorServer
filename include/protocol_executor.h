#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdbool.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct UserCommand {
    char * cmd; // command for executor: issueJob, poll. exit etc...
    char * text; // content of the bash command: ls, wget ...
    int value;
} UserCommand;

int write_all(int fd, void *buff, size_t size);

int read_all(int fd, void *buff, size_t size);

int send_message(int fd, char * message);

char * receive_message(int fd);

void send_command_issue_job(int fd, char * bash_command);

void send_command_set_concurrency(int fd, int concurrency);

void send_command_stop(int fd, char * jobid);

void send_command_poll(int fd, bool running);

void send_command_exit(int fd);


UserCommand parse_command(char * message);

#endif /* PROTOCOL_H */
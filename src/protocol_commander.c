#include <stdbool.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../include/protocol_commander.h"
int write_all(int fd, void *buff, size_t size) {

    int sent = 0;
    int n = 0;

    bool error = false;

    int remaining = size;

    while (sent < size) {
        if ((n = write(fd, buff + sent, remaining)) == -1) {
            error = true;
            break;
        }

        sent += n;

        remaining = remaining - n;
    }

    if (!error) {
        return sent;
    } else {
        return -1;
    }
}

int read_all(int fd, void *buff, size_t size) {
    int received = 0;
    int n = 0;

    bool error = false;

    int remaining = size;

    while (received < size) {
        if ((n = read(fd, buff + received, remaining)) == -1) {
            error = true;
            break;
        }

        received += n;

        remaining = remaining - n;
    }

    if (!error) {
        return received;
    } else {
        return -1;
    }
}

int send_message(int fd, char * message) {
    int n = strlen(message);

    write_all(fd, &n, sizeof (n));

    write_all(fd, message, n);

    return 0;
}

char * receive_message(int fd) {
    int n;

    if (read_all(fd, &n, sizeof (n)) <= 0) {
        perror("Warning: read_all 0 or -1 ");
        return 0;
    }

    char * buffer = calloc(n + 1, sizeof (char));

    if (read_all(fd, buffer, n) <= 0) {
        perror("Warning: read_all 0 or -1 ");
        return 0;
    }

    return buffer;
}

void send_command_issue_job(int fd, char * bash_command) {
    char * cmd = "ISSUEJOB";

    int message_length = strlen(cmd) + strlen(bash_command) + 1 + 1;

    char * buffer = calloc(message_length, sizeof (char));

    memset(buffer, 0, message_length);

    strcpy(buffer, cmd);
    strcat(buffer, " ");
    strcat(buffer, bash_command);

    send_message(fd, buffer);

    free(buffer);
}

void send_command_set_concurrency(int fd, int concurrency) {
    char * cmd = "SET_CONCURRENCY";

    char buffer_confurrency[20] = {0};

    sprintf(buffer_confurrency, "%d", concurrency);

    int message_length = strlen(cmd) + strlen(buffer_confurrency) + 1 + 1;

    char * buffer = calloc(message_length, sizeof (char));

    memset(buffer, 0, message_length);

    strcpy(buffer, cmd);

    strcat(buffer, " ");
    strcat(buffer, buffer_confurrency);

    send_message(fd, buffer);

    free(buffer);
}

void send_command_stop(int fd, char * jobid) {
    char * cmd = "STOP";

    int message_length = strlen(cmd) + strlen(jobid) + 1 + 1;

    char * buffer = calloc(message_length, sizeof (char));

    memset(buffer, 0, message_length);

    strcpy(buffer, cmd);

    strcat(buffer, " ");
    strcat(buffer, jobid);

    send_message(fd, buffer);

    free(buffer);
}

void send_command_poll(int fd, bool running) { // running: true,  queued: false
    char * cmd;

    if (running) {
        cmd = "POLL_RUNNING";
    } else {
        cmd = "POLL_QUEUED";
    }

    int message_length = strlen(cmd) + 1;

    char * buffer = calloc(message_length, sizeof (char));

    memset(buffer, 0, message_length);

    strcpy(buffer, cmd);

    send_message(fd, buffer);

    free(buffer);
}

void send_command_exit(int fd) { // running: true,  queued: false
    char * cmd = "EXIT";

    int message_length = strlen(cmd) + 1;

    char * buffer = calloc(message_length, sizeof (char));

    memset(buffer, 0, message_length);

    strcpy(buffer, cmd);

    send_message(fd, buffer);

    free(buffer);
}

// * message types:
// ISSUEJOB ls -a
// SET_CONCURRENCY 4
// STOP 5
// POLL_RUNNING 5
// POLL_PENDING 5
// EXIT

UserCommand parse_command(char * message) {
    printf("PARSING MESSAGE: %s \n", message);

    if (strncmp(message, "ISSUEJOB", strlen("ISSUEJOB")) == 0) {
        char * cmd = "ISSUEJOB";
        char * bash_command = message + strlen("ISSUEJOB") + 1;

        printf("cmd   = %s\n", cmd);
        printf("param = %s\n", bash_command);

        UserCommand result;
        result.cmd = cmd;
        result.text = bash_command;
        result.value = 0;
        return result;
    }

    if (strncmp(message, "SET_CONCURRENCY", strlen("SET_CONCURRENCY")) == 0) {
        char * cmd = "SET_CONCURRENCY";
        char * param = message + strlen("SET_CONCURRENCY") + 1;
        int concurrency = atoi(param);

        printf("cmd   = %s\n", cmd);
        printf("param = %d\n", concurrency);

        UserCommand result;
        result.cmd = cmd;
        result.text = NULL;
        result.value = concurrency;
        return result;
    }

    if (strncmp(message, "STOP", strlen("STOP")) == 0) {
        char * cmd = "STOP";
        char * jobid = message + strlen("STOP") + 1;

        printf("cmd   = %s\n", cmd);
        printf("param = %s\n", jobid);

        UserCommand result;
        result.cmd = cmd;
        result.text = jobid;
        result.value = 0;
        return result;
    }

    if (strncmp(message, "POLL_RUNNING", strlen("POLL_RUNNING")) == 0) {
        char * cmd = "POLL_RUNNING";

        printf("cmd   = %s\n", cmd);

        UserCommand result;
        result.cmd = cmd;
        result.text = NULL;
        result.value = 0;
        return result;
    }

    if (strncmp(message, "POLL_QUEUED", strlen("POLL_QUEUED")) == 0) {
        char * cmd = "POLL_QUEUED";

        printf("cmd   = %s\n", cmd);

        UserCommand result;
        result.cmd = cmd;
        result.text = NULL;
        result.value = 0;
        return result;
    }

    if (strncmp(message, "EXIT", strlen("EXIT")) == 0) {
        char * cmd = "EXIT";

        printf("cmd   = %s\n", cmd);

        UserCommand result;
        result.cmd = cmd;
        result.text = NULL;
        result.value = 0;
        return result;
    }

    printf("### Parsed failed ### \n");

    exit(222);
}
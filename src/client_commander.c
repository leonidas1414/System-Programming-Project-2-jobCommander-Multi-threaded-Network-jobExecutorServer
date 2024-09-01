#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "../include/client_commander.h"
#include "../include/protocol_commander.h"

#include <sys/socket.h>      /* sockets */
#include <netinet/in.h>      /* internet sockets */
#include <netdb.h>          /* gethostbyaddr */
#include <string.h>          /* strlen */
bool ctrl_c_pressed = false;

static const char * _ip = 0;
static int _portNum = 0;

void client_shutdown_activated(int signum) {
    printf("Received signal: %d\n", signum);
    ctrl_c_pressed = true;
}

void client_start_up(const char * ip, int portNum) {
    printf("Starting client ... \n");

    _ip = ip;
    _portNum = portNum;

    // TCP


    printf("Detected: Server online ... \n");

    struct sigaction sa = {0};
    sa.sa_handler = client_shutdown_activated;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
}

void client_service(char * execute_command, UserCommand command) {
    printf("Opening output pipe ... \n");



    struct sockaddr_in server;
    struct sockaddr *serverptr = (struct sockaddr*) &server;
    struct hostent *rem;
    int sock;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(sock);
    }

    if ((rem = gethostbyname(_ip)) == NULL) {
        herror("gethostbyname");
        exit(1);
    } else {
        memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
        server.sin_family = AF_INET; /* Internet domain */
        server.sin_port = htons(_portNum);
    }

    if (connect(sock, serverptr, sizeof (server)) < 0) {
        perror("connect");
        exit(sock);
    }

    int fd_in = sock;
    int fd_out = sock;

    bool accept_file = false;

    if (strcmp(command.cmd, "ISSUEJOB") == 0) {
        send_command_issue_job(fd_out, command.text);

        accept_file = true;
    }

    if (strcmp(command.cmd, "SET_CONCURRENCY") == 0) {
        send_command_set_concurrency(fd_out, command.value);
    }

    if (strcmp(command.cmd, "STOP") == 0) {
        send_command_stop(fd_out, command.text);
    }

    if (strcmp(command.cmd, "POLL_RUNNING") == 0) {
        send_command_poll(fd_out, true);
    }

    if (strcmp(command.cmd, "POLL_QUEUED") == 0) {
        send_command_poll(fd_out, false);
    }

    if (strcmp(command.cmd, "EXIT") == 0) {
        send_command_exit(fd_out);
    }

    char * reply = receive_message(fd_in);

    printf("> %s \n", reply);

    free(reply);

    char block[1024];

    if (accept_file) {
        printf("----- output start ------ \n");

        while (1) {
            int n = read(sock, block, sizeof (block));

            if (n <= 0) {
                break;
            }

            n = write_all(1, block, n);

            if (n <= 0) {
                break;
            }
        }

        printf("\n");

        printf("-----  output end ------ \n");
    }

    close(sock);
}

void client_stop() {
    printf("Stopping client ... \n");
}
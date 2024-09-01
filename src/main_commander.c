#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include "../include/client_commander.h"
#include "../include/protocol_commander.h"


int main(int argc, char** argv) {
    if (argc < 4) {
        printf("Error: no command is given \n");
        return 1;
    }

    char message[PATH_MAX] = {};
    char * executor_command = NULL; // issuejob, stop, poll, ....

    if (strcmp(argv[3], "issueJob") == 0) {
        executor_command = "ISSUEJOB";
    }

    if (strcmp(argv[3], "setConcurrency") == 0) {
        executor_command = "SET_CONCURRENCY";
    }

    if (strcmp(argv[3], "stop") == 0) {
        executor_command = "STOP";
    }

    if (strcmp(argv[3], "poll") == 0) {
        executor_command = "POLL_QUEUED";
    }
    
//    if (strcmp(argv[3], "poll") == 0 && strcmp(argv[4], "running") == 0) {
//        executor_command = "POLL_RUNNING";
//    }

//    if (strcmp(argv[3], "poll") == 0 && strcmp(argv[4], "queued") == 0) {
//        executor_command = "POLL_QUEUED";
//    }

    if (strcmp(argv[3], "exit") == 0) {
        executor_command = "EXIT";
    }

    if (executor_command == NULL) {
        printf("Unknown executor command \n");
        exit(1);
    }

    strcpy(message, executor_command);
    strcat(message, " ");

    int startFrom = 4;

    if (strcmp(argv[3], "poll") == 0) {
        startFrom++;
    }

    for (int i = startFrom; i < argc; i++) {
        strcat(message, argv[i]);

        if (i != argc - 1) {
            strcat(message, " ");
        }
    }

    const char * ip = argv[1];
    int portNum = atoi(argv[2]);

    if (portNum <= 1024 || portNum > 65535) {
        printf("Error: invalid port number. Valid values 1025 - 65535 \n");
        return 1;
    }

    printf("Server destination: %s:%d \n", ip, portNum);
    printf("Message: %s \n", message);

    printf("Parsing : ... \n");

    UserCommand userCommand = parse_command(message);

    printf("Parsing test: [%s] [%s] [%d] \n", userCommand.cmd, userCommand.text, userCommand.value);

    client_start_up(ip, portNum);

    client_service(message, userCommand);

    client_stop();

    return (EXIT_SUCCESS);
}
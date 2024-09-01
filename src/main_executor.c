#include <stdio.h>
#include <stdlib.h>

#include "../include/server_executor.h"


int main(int argc, char** argv) {
    if (argc != 4) {
        printf("Error: no arguments given \n");
        return 1;
    }
    
    int portNum; 
    int bufferSize;
    int threadPoolSize; 
    
    portNum = atoi(argv[1]);
    
    bufferSize = atoi(argv[2]);
    
    threadPoolSize = atoi(argv[3]);
    
    
    if (portNum <= 1024 || portNum > 65535) {
        printf("Error: invalid port number. Valid values 1025 - 65535 \n");
        return 1;
    }
    
    if (bufferSize < 1) {
        printf("Error: invalid bufferSize number. Valid values 1+ \n");
        return 1;
    }
    
    if (threadPoolSize < 1 || threadPoolSize > 100) {
        printf("Error: invalid threadPoolSize number. Valid values 1 - 100 \n");
        return 1;
    }
    
    server_start_up(portNum, bufferSize, threadPoolSize);
    
    server_service();
    
    server_stop();

    printf("Shutdown complete. \n");

    return 0;
}
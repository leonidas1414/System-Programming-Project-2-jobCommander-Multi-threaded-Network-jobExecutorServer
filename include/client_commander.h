
#ifndef CLIENT_H
#define CLIENT_H

#include <limits.h>

#include "protocol_commander.h"

void client_start_up(const char * ip, int portNum);

void client_stop();

void client_service(char * execute_command, UserCommand command);

#endif /* CLIENT_H */
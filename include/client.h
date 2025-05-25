#ifndef CLIENT_H
#define CLIENT_H

#include "common.h"


void start_client(const char *server_ip);

void *client_receive_handler(void *arg);

void *client_send_handler(void *arg);

#endif 

#ifndef SERVER_H
#define SERVER_H

#include "common.h"

// Starts the server, listens for incoming client connections
void start_server(void);

// Handler function for receiving messages from the client
void *server_receive_handler(void *arg);

// Handler function for sending messages to the client
void *server_send_handler(void *arg);

#endif // SERVER_H

// client_manager.h
#ifndef CLIENT_MANAGER_H
#define CLIENT_MANAGER_H

#include <pthread.h>
#include "common.h"

#define MAX_CLIENTS 100
#define USERNAME_MAX 32
#define STATUS_MAX 16
#define BUFFER_SIZE 1024

extern Client_t clients[MAX_CLIENTS];
extern pthread_mutex_t clients_mutex;


void init_clients();


void add_client(int socket, const char *username, const char *status);


void remove_client(int socket);


int find_client_index(int socket);


void broadcast_message(const char *message);


void send_message(int socket, const char *message);


int send_private_message(const char *target_username, const char *message);

#endif 

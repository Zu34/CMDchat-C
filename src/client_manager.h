#ifndef CLIENT_MANAGER_H
#define CLIENT_MANAGER_H

#include <pthread.h>

#define MAX_CLIENTS 100
#define USERNAME_MAX 32
#define STATUS_MAX 16
#define BUFFER_SIZE 1024

typedef struct {
    int socket;
    char username[USERNAME_MAX];
    char status[STATUS_MAX];
    int active;
} Client;

extern Client clients[MAX_CLIENTS];
extern pthread_mutex_t clients_mutex;

void add_client(int sock, const char *username, const char *status);

// Remove by socket
void remove_client(int sock);

// Find client index by socket, -1 if not found
int find_client_index(int sock);

// Broadcast a message to all clients
void broadcast_message(const char *msg);

// Send a message to a single client
void send_message(int sock, const char *msg);

// Initialize clients array
void init_clients();

#endif

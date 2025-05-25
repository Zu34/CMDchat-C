#ifndef COMMON_H
#define COMMON_H

#include <netinet/in.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define USERNAME_MAX 32
#define HISTORY_SIZE 100
#define MAX_CLIENTS 10

typedef struct {
    int socket;
    char username[USERNAME_MAX];
    char status[16]; // online, away, busy
    int active; // 1 if connected, 0 if free slot
} client_t;

extern client_t clients[MAX_CLIENTS];
extern pthread_mutex_t clients_mutex;

#define HANDLE_ERROR(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

#endif // COMMON_H


#ifndef COMMON_H
#define COMMON_H

#include <netinet/in.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define USERNAME_MAX 32
#define STATUS_MAX 16
#define ROOM_NAME_MAX 32
#define MAX_CLIENTS 100
#define MAX_MESSAGES 1000
#define MAX_REACTIONS 10
#define EMOJI_MAX 8


typedef struct {
    int socket;
    char username[USERNAME_MAX];
    char status[STATUS_MAX]; // online, away, busy
    char current_room[ROOM_NAME_MAX];
    int active; 
} Client_t;


typedef struct {
    int id;
    char sender[USERNAME_MAX];
    char text[BUFFER_SIZE];
    char timestamp[32];

    struct {
        char emoji[EMOJI_MAX];
        int count;
    } reactions[MAX_REACTIONS];

    int reaction_count;
} ChatMessage;





extern int client_socket;
extern char client_username[USERNAME_MAX];
extern int client_connected;

extern Client_t clients[MAX_CLIENTS];
extern pthread_mutex_t clients_mutex;

#define HANDLE_ERROR(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

#endif

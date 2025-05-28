// common.h
#ifndef COMMON_H
#define COMMON_H

#include <netinet/in.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define USERNAME_MAX 32
#define STATUS_MAX 16
#define ROOM_NAME_MAX 32
#define HISTORY_SIZE 100
#define MAX_CLIENTS 100
#define MAX_ROOMS 50
#define ROOM_USER_LIMIT 10

typedef struct {
    int socket;
    char username[USERNAME_MAX];
    char status[STATUS_MAX]; // online, away, busy
    char current_room[ROOM_NAME_MAX]; // room the client is currently in
    int active; 
} Client_t;

typedef struct {
    char name[ROOM_NAME_MAX];
    int users[MAX_CLIENTS]; // array of socket descriptors
    int user_count;
    int user_limit;
    int active;
} Room_t;

// Only if needed in other files (not required if just in server.c)
extern int client_socket;
extern char client_username[USERNAME_MAX];
extern int client_connected;


// Shared structures
extern Client_t clients[MAX_CLIENTS];
extern pthread_mutex_t clients_mutex;

extern Room_t rooms[MAX_ROOMS];
extern pthread_mutex_t rooms_mutex;

// Room management
void init_rooms();
int join_room(int client_socket, const char *room_name, int limit_if_new);
void leave_room(int client_socket, const char *room_name);
void broadcast_to_room(const char *room_name, const char *message);
void list_rooms(int client_socket);

#define HANDLE_ERROR(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

#endif 

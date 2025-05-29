#ifndef ROOM_MANAGER_H
#define ROOM_MANAGER_H

#include <pthread.h>
#include "common.h"  

#define MAX_ROOMS 50
#define ROOM_USER_LIMIT 10
#define MAX_MESSAGES 100
#define CREATOR_NAME_MAX 32



// typedef struct {
//     char content[BUFFER_SIZE];
//     char sender[USERNAME_MAX];
//     time_t timestamp;
// } ChatMessage;

typedef struct {
    char name[ROOM_NAME_MAX];
    int users[MAX_CLIENTS]; // socket descriptors
    int user_count;
    int user_limit;
    ChatMessage messages[MAX_MESSAGES];
    char creator[CREATOR_NAME_MAX];  
    int active;
    int message_count;

} Room_t;

extern Room_t rooms[MAX_ROOMS];
extern pthread_mutex_t rooms_mutex;

// Room management functions
void init_rooms();
int join_room(int client_socket, const char *room_name, int limit_if_new);
void leave_room(int client_socket, const char *room_name);
void broadcast_to_room(const char *room_name, const char *message);
void list_rooms(int client_socket);

#endif

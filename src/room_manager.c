// room_manager.c
#include "room_manager.h"
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>

#include "common.h"
#include "client_manager.h"  // for find_client_index()

Room_t rooms[MAX_ROOMS];
pthread_mutex_t rooms_mutex = PTHREAD_MUTEX_INITIALIZER;

void init_rooms() {
    pthread_mutex_lock(&rooms_mutex);
    for (int i = 0; i < MAX_ROOMS; i++) {
        rooms[i].active = 0;
        rooms[i].user_count = 0;
        rooms[i].user_limit = ROOM_USER_LIMIT;
        rooms[i].name[0] = '\0';
        for (int j = 0; j < MAX_CLIENTS; j++) {
            rooms[i].users[j] = -1;
        }
    }
    pthread_mutex_unlock(&rooms_mutex);
}

int join_room(int client_socket, const char *room_name, int limit_if_new) {
    pthread_mutex_lock(&rooms_mutex);
    int i, room_index = -1;

    for (i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].active && strcmp(rooms[i].name, room_name) == 0) {
            room_index = i;
            break;
        }
    }

    if (room_index == -1) {
        for (i = 0; i < MAX_ROOMS; i++) {
            if (!rooms[i].active) {
                strncpy(rooms[i].name, room_name, ROOM_NAME_MAX - 1);
                rooms[i].name[ROOM_NAME_MAX - 1] = '\0';
                rooms[i].active = 1;
                rooms[i].user_count = 0;
                rooms[i].user_limit = limit_if_new;
    
                int client_index = find_client_index(client_socket);
                if (client_index != -1) {
                    strncpy(rooms[i].creator, clients[client_index].username, CREATOR_NAME_MAX - 1);
                    rooms[i].creator[CREATOR_NAME_MAX - 1] = '\0';
                } else {
                    strncpy(rooms[i].creator, "Unknown", CREATOR_NAME_MAX - 1);
                    rooms[i].creator[CREATOR_NAME_MAX - 1] = '\0';
                }
    
                room_index = i;
                break;
            }
        }
    

        if (room_index == -1) {
            pthread_mutex_unlock(&rooms_mutex);
            return -1;
        }
    }

    if (rooms[room_index].user_count >= rooms[room_index].user_limit) {
        pthread_mutex_unlock(&rooms_mutex);
        return -2;
    }

    rooms[room_index].users[rooms[room_index].user_count++] = client_socket;

    int client_index = find_client_index(client_socket);
    if (client_index != -1) {
        strncpy(clients[client_index].current_room, room_name, ROOM_NAME_MAX - 1);
        clients[client_index].current_room[ROOM_NAME_MAX - 1] = '\0';
    }

    pthread_mutex_unlock(&rooms_mutex);
    return 0;
}

void leave_room(int client_socket, const char *room_name) {
    pthread_mutex_lock(&rooms_mutex);
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].active && strcmp(rooms[i].name, room_name) == 0) {
            int found = 0;
            for (int j = 0; j < rooms[i].user_count; j++) {
                if (rooms[i].users[j] == client_socket) {
                    for (int k = j; k < rooms[i].user_count - 1; k++) {
                        rooms[i].users[k] = rooms[i].users[k + 1];
                    }
                    rooms[i].users[rooms[i].user_count - 1] = -1;
                    rooms[i].user_count--;
                    found = 1;
                    break;
                }
            }

            if (found && rooms[i].user_count == 0) {
                rooms[i].active = 0;
            }

            break;
        }
    }

    int client_index = find_client_index(client_socket);
    if (client_index != -1) {
        clients[client_index].current_room[0] = '\0';
    }

    pthread_mutex_unlock(&rooms_mutex);
}

void broadcast_to_room(const char *room_name, const char *message) {
    pthread_mutex_lock(&rooms_mutex);
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].active && strcmp(rooms[i].name, room_name) == 0) {
            for (int j = 0; j < rooms[i].user_count; j++) {
                int client_socket = rooms[i].users[j];
                send(client_socket, message, strlen(message), 0);
            }
            break;
        }
    }
    pthread_mutex_unlock(&rooms_mutex);
}

void list_rooms(int client_socket) {
    pthread_mutex_lock(&rooms_mutex);
    char msg[BUFFER_SIZE];
    strcpy(msg, "Active Rooms:\n");

    for (int i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].active) {
            char line[128];
            snprintf(line, sizeof(line), " - %s (%d/%d users) - Created by %s\n",
            rooms[i].name, rooms[i].user_count, rooms[i].user_limit, rooms[i].creator);
   
            strncat(msg, line, sizeof(msg) - strlen(msg) - 1);
        }
    }

    pthread_mutex_unlock(&rooms_mutex);
    send(client_socket, msg, strlen(msg), 0);
}

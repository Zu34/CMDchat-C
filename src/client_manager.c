#include "client_manager.h"
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include "common.h"



void init_clients() {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].active = 0;
        clients[i].socket = -1;
        clients[i].username[0] = '\0';
        clients[i].status[0] = '\0';
    }
    pthread_mutex_unlock(&clients_mutex);
}

void add_client(int sock, const char *username, const char *status) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active) {
            clients[i].socket = sock;
            strncpy(clients[i].username, username, USERNAME_MAX - 1);
            clients[i].username[USERNAME_MAX - 1] = '\0';
            strncpy(clients[i].status, status, STATUS_MAX - 1);
            clients[i].status[STATUS_MAX - 1] = '\0';
            clients[i].active = 1;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void remove_client(int sock) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].socket == sock) {
            clients[i].active = 0;
            close(clients[i].socket);
            clients[i].socket = -1;
            clients[i].username[0] = '\0';
            clients[i].status[0] = '\0';
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

int find_client_index(int sock) {
    int idx = -1;
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].socket == sock) {
            idx = i;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    return idx;
}

void broadcast_message(const char *msg) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active) {
            send(clients[i].socket, msg, strlen(msg), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void send_message(int sock, const char *msg) {
    send(sock, msg, strlen(msg), 0);
}

// New function: send a private message to a specific user
int send_private_message(const char *target_username, const char *message) {
    int sent = 0;
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && strcmp(clients[i].username, target_username) == 0) {
            if (send(clients[i].socket, message, strlen(message), 0) >= 0) {
                sent = 1;
            }
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    return sent;  // 1 if sent successfully, 0 if user not found
}

// New function: get the status of a user by username
const char *get_user_status(const char *username) {
    const char *status = NULL;
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && strcmp(clients[i].username, username) == 0) {
            status = clients[i].status;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    return status; // NULL if user not found
}

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

    // Check if the room exists
    for (i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].active && strcmp(rooms[i].name, room_name) == 0) {
            room_index = i;
            break;
        }
    }

    // Create the room if not found
    if (room_index == -1) {
        for (i = 0; i < MAX_ROOMS; i++) {
            if (!rooms[i].active) {
                strncpy(rooms[i].name, room_name, ROOM_NAME_MAX - 1);
                rooms[i].name[ROOM_NAME_MAX - 1] = '\0';
                rooms[i].active = 1;
                rooms[i].user_count = 0;
                rooms[i].user_limit = limit_if_new;
                room_index = i;
                break;
            }
        }

        if (room_index == -1) {
            pthread_mutex_unlock(&rooms_mutex);
            return -1; // No space for new room
        }
    }

    // Check if room is full
    if (rooms[room_index].user_count >= rooms[room_index].user_limit) {
        pthread_mutex_unlock(&rooms_mutex);
        return -2; // Room full
    }

    // Add client to room
    rooms[room_index].users[rooms[room_index].user_count++] = client_socket;

    // Update client room field
    int client_index = find_client_index(client_socket);
    if (client_index != -1) {
        strncpy(clients[client_index].current_room, room_name, ROOM_NAME_MAX - 1);
        clients[client_index].current_room[ROOM_NAME_MAX - 1] = '\0';
    }

    pthread_mutex_unlock(&rooms_mutex);
    return 0; // Success
}


void leave_room(int client_socket, const char *room_name) {
    pthread_mutex_lock(&rooms_mutex);

    for (int i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].active && strcmp(rooms[i].name, room_name) == 0) {
            int found = 0;
            for (int j = 0; j < rooms[i].user_count; j++) {
                if (rooms[i].users[j] == client_socket) {
                    // Shift others left
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
                // Deactivate empty room
                rooms[i].active = 0;
            }

            break;
        }
    }

    // Clear client's room field
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
            snprintf(line, sizeof(line), " - %s (%d/%d users)\n",
                     rooms[i].name, rooms[i].user_count, rooms[i].user_limit);
            strncat(msg, line, sizeof(msg) - strlen(msg) - 1);
        }
    }

    pthread_mutex_unlock(&rooms_mutex);
    send(client_socket, msg, strlen(msg), 0);
}

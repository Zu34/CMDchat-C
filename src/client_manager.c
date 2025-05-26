#include "client_manager.h"
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

Client clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

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


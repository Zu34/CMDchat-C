#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

#include "common.h"
#include "client_manager.h"
#include "utils.h"

#define MAX_BACKLOG 10

void *server_receive_handler(void *arg) {
    int client_socket = *((int *)arg);
    free(arg);

    char buffer[BUFFER_SIZE];
    char client_username[USERNAME_MAX] = {0};
    int client_connected = 0;
    int initial_handshake_done = 0;

    while (1) {
        int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            print_timestamped("Client disconnected.");
            remove_client(client_socket);
            close(client_socket);
            return NULL;
        }

        buffer[bytes_received] = '\0';
        trim_newline(buffer);

        if (!initial_handshake_done) {
            char *sep = strchr(buffer, '|');
            if (sep) {
                *sep = '\0';
                strncpy(client_username, buffer, USERNAME_MAX - 1);
                client_username[USERNAME_MAX - 1] = '\0';

                char client_status[STATUS_MAX];
                strncpy(client_status, sep + 1, STATUS_MAX - 1);
                client_status[STATUS_MAX - 1] = '\0';

                add_client(client_socket, client_username, client_status);

                char welcome_msg[BUFFER_SIZE];
                snprintf(welcome_msg, sizeof(welcome_msg),
                         "Hey %s! It's been a while. Do you want to chat? (yes/no)\n", client_username);
                send_message(client_socket, welcome_msg);
                initial_handshake_done = 1;
            } else {
                send_message(client_socket, "Invalid format. Expected: username|status\n");
            }
            continue;
        }

        if (!client_connected) {
            if (strcasecmp(buffer, "no") == 0) {
                send_message(client_socket,
                    "Okay! Here are some commands:\n"
                    "/help - Show commands\n"
                    "/list - List users\n"
                    "/quit - Disconnect\n");
                remove_client(client_socket);
                close(client_socket);
                return NULL;
            } else {
                send_message(client_socket, "Welcome to the chatroom! Type /help for commands.\n");
                client_connected = 1;
                continue;
            }
        }

        // Handle commands
        if (buffer[0] == '/') {
            if (strcmp(buffer, "/list") == 0) {
                pthread_mutex_lock(&clients_mutex);
                char list_msg[BUFFER_SIZE];
                strcpy(list_msg, "Connected users:\n");
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].active) {
                        char line[128];
                        snprintf(line, sizeof(line), " - %s [%s]\n", clients[i].username, clients[i].status);
                        strncat(list_msg, line, sizeof(list_msg) - strlen(list_msg) - 1);
                    }
                }
                pthread_mutex_unlock(&clients_mutex);
                send_message(client_socket, list_msg);
            } else if (strcmp(buffer, "/help") == 0) {
                send_message(client_socket,
                    "Commands:\n"
                    "/list - List users\n"
                    "/quit - Disconnect\n"
                    "/help - Show this message\n"
                    "/status <online|away|busy> - Update your status\n"
                    "/sendfile <filename> <size> - Send a file\n"
                    "/msg <username> <message> - Send a private message\n");
            } else if (strcmp(buffer, "/quit") == 0) {
                send_message(client_socket, "Goodbye!\n");
                remove_client(client_socket);
                close(client_socket);
                return NULL;
            } else if (strncmp(buffer, "/status ", 8) == 0) {
                char new_status[STATUS_MAX];
                if (sscanf(buffer + 8, "%15s", new_status) == 1) {
                    pthread_mutex_lock(&clients_mutex);
                    int idx = find_client_index(client_socket);
                    if (idx != -1) {
                        strncpy(clients[idx].status, new_status, STATUS_MAX - 1);
                        clients[idx].status[STATUS_MAX - 1] = '\0';

                        char status_msg[BUFFER_SIZE];
                        snprintf(status_msg, sizeof(status_msg),
                                 "%s changed status to '%s'.", clients[idx].username, new_status);
                        broadcast_message(status_msg);
                    }
                    pthread_mutex_unlock(&clients_mutex);
                    send_message(client_socket, "Status updated.\n");
                } else {
                    send_message(client_socket, "Usage: /status <online|away|busy>\n");
                }
            } else if (strncmp(buffer, "/sendfile ", 10) == 0) {
                char filename[256];
                long filesize;
                if (sscanf(buffer + 10, "%255s %ld", filename, &filesize) != 2 || filesize <= 0) {
                    send_message(client_socket, "Usage: /sendfile <filename> <filesize>\n");
                    continue;
                }

                send_message(client_socket, "Ready to receive file data\n");

                int fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
                if (fd < 0) {
                    send_message(client_socket, "Failed to open file\n");
                    continue;
                }

                long bytes_received = 0;
                while (bytes_received < filesize) {
                    int to_read = (filesize - bytes_received > BUFFER_SIZE)
                                  ? BUFFER_SIZE
                                  : (filesize - bytes_received);
                    int n = recv(client_socket, buffer, to_read, 0);
                    if (n <= 0) break;
                    write(fd, buffer, n);
                    bytes_received += n;
                }
                close(fd);

                if (bytes_received == filesize) {
                    send_message(client_socket, "File transfer complete!\n");
                } else {
                    send_message(client_socket, "File transfer interrupted.\n");
                }
            } else if (strncmp(buffer, "/msg ", 5) == 0) {
                char target[USERNAME_MAX];
                char *msg_start = buffer + 5;

                if (sscanf(msg_start, "%31s", target) == 1) {
                    char *message_body = msg_start + strlen(target) + 1;
                    if (*message_body != '\0') {
                        int sender_idx = find_client_index(client_socket);
                        if (sender_idx != -1) {
                            char full_msg[BUFFER_SIZE + USERNAME_MAX];
                            snprintf(full_msg, sizeof(full_msg),
                                     "[PM from %s]: %s\n", clients[sender_idx].username, message_body);

                            if (!send_private_message(target, full_msg)) {
                                send_message(client_socket, "User not found or not online.\n");
                            } else {
                                send_message(client_socket, "Private message sent.\n");
                            }
                        }
                    } else {
                        send_message(client_socket, "Usage: /msg <username> <message>\n");
                    }
                } else {
                    send_message(client_socket, "Usage: /msg <username> <message>\n");
                }
            } else {
                send_message(client_socket, "Unknown command. Type /help for commands.\n");
            }
            continue;
        }

        // Broadcast normal chat message
        int idx = find_client_index(client_socket);
        const char *status = (idx != -1) ? clients[idx].status : "unknown";

        char chat_msg[BUFFER_SIZE + USERNAME_MAX];
        snprintf(chat_msg, sizeof(chat_msg), "%s [%s]: %s\n", client_username, status, buffer);
        broadcast_message(chat_msg);
    }

    return NULL;
}

void start_server() {
    int server_fd, new_socket;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
        HANDLE_ERROR("socket");

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        HANDLE_ERROR("bind");

    if (listen(server_fd, MAX_BACKLOG) < 0)
        HANDLE_ERROR("listen");

    print_timestamped("Server started and listening...");

    while (1) {
        new_socket = accept(server_fd, (struct sockaddr *)&addr, &addrlen);
        if (new_socket < 0) {
            perror("accept failed");
            continue;
        }

        int *client_sock_ptr = malloc(sizeof(int));
        *client_sock_ptr = new_socket;

        pthread_t tid;
        pthread_create(&tid, NULL, server_receive_handler, client_sock_ptr);
        pthread_detach(tid);
    }
}

int main() {
    init_clients();
    start_server();
    return 0;
}

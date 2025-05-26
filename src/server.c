#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#include "client_manager.h"
#include "common.h"
#include "utils.h"  // ✅ Use utility functions here

#define PORT 12345

int client_socket = -1;
char client_username[USERNAME_MAX];
int username_received = 0;
int client_connected = 0;

void *server_receive_handler(void *arg) {
    char buffer[BUFFER_SIZE];

    while (1) {
        int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            print_timestamped("Client disconnected.");
            remove_client(client_socket);
            close(client_socket);
            client_connected = 0;
            exit(EXIT_SUCCESS);
        }

        buffer[bytes_received] = '\0';
        trim_newline(buffer);

        if (!username_received) {
            char *sep = strchr(buffer, '|');
            if (sep) {
                *sep = '\0';
                strncpy(client_username, buffer, USERNAME_MAX - 1);
                client_username[USERNAME_MAX - 1] = '\0';

                char client_status[16];
                strncpy(client_status, sep + 1, sizeof(client_status) - 1);
                client_status[sizeof(client_status) - 1] = '\0';

                add_client(client_socket, client_username, client_status);



                char welcome_msg[BUFFER_SIZE];
                snprintf(welcome_msg, sizeof(welcome_msg), "User '%s' connected with status '%s'.",
                         client_username, client_status);
                print_timestamped(welcome_msg);
            } else {
                strncpy(client_username, buffer, USERNAME_MAX - 1);
                client_username[USERNAME_MAX - 1] = '\0';
                add_client(client_socket, client_username, "online");

                char welcome_msg[BUFFER_SIZE];
                snprintf(welcome_msg, sizeof(welcome_msg), "User '%s' connected with status 'online'.",
                         client_username);
                print_timestamped(welcome_msg);
            }

            send_message(client_socket, "Welcome to the chat!\nType /help for commands.\n");
            username_received = 1;
            continue;
        }

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
                    "/list - list users\n"
                    "/quit - disconnect\n"
                    "/help - show commands\n"
                    "/status <online|away|busy> - change your status\n"
                    "/sendfile filename filesize - send file\n");
            } else if (strcmp(buffer, "/quit") == 0) {
                print_timestamped("Client requested disconnect.");
                send_message(client_socket, "Goodbye!\n");
                remove_client(client_socket);
                close(client_socket);
                client_connected = 0;
                exit(EXIT_SUCCESS);
            } else if (strncmp(buffer, "/status ", 8) == 0) {
                char new_status[16];
                if (sscanf(buffer + 8, "%15s", new_status) == 1) {
                    pthread_mutex_lock(&clients_mutex);
                    int idx = find_client_index(client_socket);
                    if (idx != -1) {
                        strncpy(clients[idx].status, new_status, STATUS_MAX - 1);
                        clients[idx].status[STATUS_MAX - 1] = '\0';

                        char status_msg[BUFFER_SIZE];
                        snprintf(status_msg, sizeof(status_msg), "%s changed status to '%s'.",
                                 clients[idx].username, new_status);
                        print_timestamped(status_msg);
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
                    send_message(client_socket, "Invalid /sendfile usage. Usage: /sendfile filename filesize\n");
                    continue;
                }

                char notify[BUFFER_SIZE];
                snprintf(notify, sizeof(notify), "Receiving file '%s' (%ld bytes)...\n", filename, filesize);
                print_timestamped(notify);
                send_message(client_socket, "Ready to receive file data\n");

                int fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
                if (fd < 0) {
                    send_message(client_socket, "Failed to open file for writing\n");
                    continue;
                }

                long bytes_received = 0;
                while (bytes_received < filesize) {
                    int to_read = (filesize - bytes_received > BUFFER_SIZE) ? BUFFER_SIZE : (filesize - bytes_received);
                    int n = recv(client_socket, buffer, to_read, 0);
                    if (n <= 0) {
                        print_timestamped("File transfer interrupted.");
                        close(fd);
                        break;
                    }
                    write(fd, buffer, n);
                    bytes_received += n;
                }
                close(fd);

                if (bytes_received == filesize) {
                    send_message(client_socket, "File transfer complete!\n");
                    char done_msg[BUFFER_SIZE];
                    snprintf(done_msg, sizeof(done_msg), "File '%s' received successfully.", filename);
                    print_timestamped(done_msg);
                }
            } else {
                send_message(client_socket, "Unknown command. Type /help for commands.\n");
            }
            continue;
        }

        // Normal message
        char formatted_msg[BUFFER_SIZE + USERNAME_MAX];
        int idx = find_client_index(client_socket);
        const char *status = (idx != -1) ? clients[idx].status : "unknown";
        snprintf(formatted_msg, sizeof(formatted_msg), "%s [%s]: %s\n", client_username, status, buffer);
        print_timestamped(formatted_msg);
        broadcast_message(formatted_msg);
    }

    return NULL;
}








void start_server() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
        HANDLE_ERROR("socket failed");

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
        HANDLE_ERROR("setsockopt");

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
        HANDLE_ERROR("bind failed");

    if (listen(server_fd, 3) < 0)
        HANDLE_ERROR("listen");

    print_timestamped("Server started and listening...");

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
            HANDLE_ERROR("accept");

        pthread_mutex_lock(&clients_mutex);
        if (client_connected) {
            pthread_mutex_unlock(&clients_mutex);
            const char *msg = "Server is full. Try later.\n";
            send(new_socket, msg, strlen(msg), 0);
            close(new_socket);
            continue;
        }
        client_connected = 1;
        pthread_mutex_unlock(&clients_mutex);

        client_socket = new_socket;
        username_received = 0;

        pthread_t tid;
        pthread_create(&tid, NULL, server_receive_handler, NULL);
    }
}

int main() {
    init_clients(); 
    start_server();
    return 0;
}

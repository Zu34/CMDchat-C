#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "common.h"

int sock = 0;
char username[USERNAME_MAX];






void add_client(int sock, const char *username) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active) {
            clients[i].socket = sock;
            strncpy(clients[i].username, username, USERNAME_MAX);
            strncpy(clients[i].status, "online", sizeof(clients[i].status));
            clients[i].active = 1;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Remove client from list
void remove_client(int sock) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].socket == sock) {
            clients[i].active = 0;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Find client index by socket
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

// Broadcast message to all clients
void broadcast_message(const char *msg) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active) {
            send(clients[i].socket, msg, strlen(msg), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void trim_newline(char *str) {
    size_t len = strlen(str);
    if (len > 0 && str[len -1] == '\n')
        str[len - 1] = '\0';
}

void print_timestamped(const char *msg) {
    char buffer[BUFFER_SIZE];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    snprintf(buffer, sizeof(buffer), "[%02d:%02d:%02d] %s\n", t->tm_hour, t->tm_min, t->tm_sec, msg);
    printf("%s", buffer);
}

void *client_receive_handler(void *arg) {
    char buffer[BUFFER_SIZE];
    while (1) {
        int bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            print_timestamped("Server disconnected.");
            close(sock);
            exit(EXIT_SUCCESS);
        }
        buffer[bytes_received] = '\0';
        printf("%s", buffer);
    }
    return NULL;
}

void send_file(const char *filename) {
    // Open file
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        printf("Failed to open file %s\n", filename);
        return;
    }

    // Get file size
    struct stat st;
    if (fstat(fd, &st) < 0) {
        printf("Failed to get file size\n");
        close(fd);
        return;
    }
    long filesize = st.st_size;

    // Send command to server to start file transfer
    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "/sendfile %s %ld", filename, filesize);
    send(sock, command, strlen(command), 0);

    // Wait for server to say ready
    char buffer[BUFFER_SIZE];
    int n = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0) {
        printf("Server disconnected during file transfer.\n");
        close(fd);
        return;
    }
    buffer[n] = '\0';
    if (strstr(buffer, "Ready") == NULL) {
        printf("Server denied file transfer: %s\n", buffer);
        close(fd);
        return;
    }

    // Send file data in chunks
    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        send(sock, buffer, bytes_read, 0);
    }
    close(fd);

    // Wait for server to confirm completion
    n = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (n > 0) {
        buffer[n] = '\0';
        printf("%s", buffer);
    }
}


void *client_send_handler(void *arg) {
    char buffer[BUFFER_SIZE];
    char status[16] = "online";  // Default status

    // Send username and initial status on connect
    char user_status_msg[USERNAME_MAX + 16];
    snprintf(user_status_msg, sizeof(user_status_msg), "%s|%s", username, status);
    send(sock, user_status_msg, strlen(user_status_msg), 0);

    printf("Wanna chat?\n");

    while (1) {
        if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
            trim_newline(buffer);
            if (strlen(buffer) == 0) continue;

            if (buffer[0] == '/') {
                // Local /help handling:
                if (strcmp(buffer, "/help") == 0) {
                    printf("Commands:\n /list - list users\n /quit - exit\n /help - show commands\n /sendfile filename - send file\n /status <online|away|busy> - change status\n");
                    continue;
                }
                
                // Handle /status command
                if (strncmp(buffer, "/status ", 8) == 0) {
                    char new_status[16];
                    if (sscanf(buffer + 8, "%15s", new_status) == 1) {
                        // Update local status variable
                        strncpy(status, new_status, sizeof(status));
                        status[sizeof(status) - 1] = '\0';

                        // Send status update to server
                        char status_msg[BUFFER_SIZE];
                        snprintf(status_msg, sizeof(status_msg), "/status %s", status);
                        send(sock, status_msg, strlen(status_msg), 0);

                        printf("Status updated to '%s'\n", status);
                    } else {
                        printf("Usage: /status <online|away|busy>\n");
                    }
                    continue;
                }

                // Handle /sendfile command
                if (strncmp(buffer, "/sendfile ", 10) == 0) {
                    char filename[256];
                    if (sscanf(buffer + 10, "%s", filename) == 1) {
                        send_file(filename);
                    } else {
                        printf("Usage: /sendfile filename\n");
                    }
                    continue;
                }
            }

            // Normal send
            send(sock, buffer, strlen(buffer), 0);

            // Quit locally on /quit to avoid hanging after server closes
            if (strcmp(buffer, "/quit") == 0) {
                printf("Disconnecting...\n");
                close(sock);
                exit(EXIT_SUCCESS);
            }
        }
    }
    return NULL;
}


void start_client(const char *server_ip) {
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        HANDLE_ERROR("socket failed");

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0)
        HANDLE_ERROR("Invalid address / Address not supported");

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        HANDLE_ERROR("Connection Failed");

        printf("[connected to server]\n");
        printf("Wanna chat?\n\n");

        printf("Welcome to the chat!\n");
        printf("Type /help for commands.\n");
        printf("/help\n\n");


    pthread_t recv_thread, send_thread;
    pthread_create(&recv_thread, NULL, client_receive_handler, NULL);
    pthread_create(&send_thread, NULL, client_send_handler, NULL);

    pthread_join(recv_thread, NULL);
    pthread_join(send_thread, NULL);

    close(sock);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <server_ip> <username>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    strncpy(username, argv[2], USERNAME_MAX - 1);
    username[USERNAME_MAX - 1] = '\0';

    start_client(argv[1]);
    return 0;
}

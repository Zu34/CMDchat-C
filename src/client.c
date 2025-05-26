

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "common.h"
#include "client_manager.h"

#define PORT 12345
#define SERVER_IP "127.0.0.1"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 12345
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 1024
#define USERNAME_MAX 32

int sock;

void *recv_handler(void *arg) {
    char buffer[BUFFER_SIZE];
    while (1) {
        int bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            printf("\nDisconnected from server.\n");
            close(sock);
            exit(EXIT_FAILURE);
        }
        buffer[bytes_received] = '\0';
        printf("%s", buffer);
        fflush(stdout);
    }
    return NULL;
}

int main() {
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    char username[USERNAME_MAX];

    printf("Enter username: ");
    if (!fgets(username, sizeof(username), stdin)) {
        fprintf(stderr, "Failed to read username.\n");
        return 1;
    }
    username[strcspn(username, "\n")] = '\0';

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(sock);
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connect failed");
        close(sock);
        return 1;
    }

    // Send username and default status
    snprintf(buffer, sizeof(buffer), "%s|online\n", username);
    send(sock, buffer, strlen(buffer), 0);

    // Start the receiver thread
    pthread_t recv_thread;
    if (pthread_create(&recv_thread, NULL, recv_handler, NULL) != 0) {
        fprintf(stderr, "Failed to create receive thread\n");
        close(sock);
        return 1;
    }

    printf("Welcome, %s! Type messages or commands. /quit to exit.\n", username);

    // Main loop to send messages
    while (1) {
        if (!fgets(buffer, sizeof(buffer), stdin)) {
            break; // EOF or error
        }

        // Trim newline
        buffer[strcspn(buffer, "\n")] = '\0';

        if (strlen(buffer) == 0) continue; // ignore empty

        if (strcmp(buffer, "/quit") == 0) {
            send(sock, buffer, strlen(buffer), 0);
            break;
        }

        // Send message or command
        send(sock, buffer, strlen(buffer), 0);
    }

    printf("Disconnecting...\n");
    close(sock);
    pthread_cancel(recv_thread);
    pthread_join(recv_thread, NULL);

    return 0;
}

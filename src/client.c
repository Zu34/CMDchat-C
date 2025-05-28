#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <getopt.h>

#include "common.h"

int sock;

void print_usage(const char *prog) {
    printf("Usage: %s [-h] -a <server_ip> -p <port> -u <username>\n", prog);
    printf("Options:\n");
    printf("  -h           Show this help message and exit\n");
    printf("  -a <ip>      IP address of the server [required]\n");
    printf("  -p <port>    Port number of the server [required]\n");
    printf("  -u <name>    Username [required]\n");
}

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

int main(int argc, char *argv[]) {
    char *server_ip = NULL;
    int port = 0;
    char username[USERNAME_MAX] = {0};

    int opt;
    while ((opt = getopt(argc, argv, "ha:p:u:")) != -1) {
        switch (opt) {
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 'a':
                server_ip = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'u':
                strncpy(username, optarg, USERNAME_MAX - 1);
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    if (!server_ip || !port || username[0] == '\0') {
        fprintf(stderr, "Missing required arguments.\n");
        print_usage(argv[0]);
        return 1;
    }

    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid server IP address");
        close(sock);
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connect failed");
        close(sock);
        return 1;
    }

    // Send initial username and status
    snprintf(buffer, sizeof(buffer), "%s|online\n", username);
    send(sock, buffer, strlen(buffer), 0);

    // Receive welcome message and ask to chat
    printf("Hey %s! It's been a while. Do you want to chat? (yes/no): ", username);
    fflush(stdout);
    fgets(buffer, sizeof(buffer), stdin);
    buffer[strcspn(buffer, "\n")] = '\0';

    if (strcasecmp(buffer, "no") == 0) {
        printf("Okay! Here are the available commands:\n");
        printf("/help - Show command list\n");
        printf("/list - List users\n");
        printf("/quit - Disconnect\n");
        return 0;
    }

    // Start receiver thread
    pthread_t recv_thread;
    if (pthread_create(&recv_thread, NULL, recv_handler, NULL) != 0) {
        fprintf(stderr, "Failed to create receiver thread\n");
        close(sock);
        return 1;
    }

    printf("Welcome to the chatroom, %s! Type /help for commands.\n", username);

    // Chat input loop
    while (1) {
        if (!fgets(buffer, sizeof(buffer), stdin)) break;

        buffer[strcspn(buffer, "\n")] = '\0';
        if (strlen(buffer) == 0) continue;

        if (strcmp(buffer, "/quit") == 0) {
            send(sock, buffer, strlen(buffer), 0);
            break;
        }

        send(sock, buffer, strlen(buffer), 0);
    }

    printf("Disconnecting...\n");
    close(sock);
    pthread_cancel(recv_thread);
    pthread_join(recv_thread, NULL);

    return 0;
}

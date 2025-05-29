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
#include "room_manager.h"
#include "utils.h"

#define MAX_BACKLOG 10

void *server_receive_handler(void *arg) {
    int client_socket = *((int *)arg);
    free(arg);

    char buffer[BUFFER_SIZE];
    int initial_handshake_done = 0;

    char client_username[USERNAME_MAX];

    while (1) {
        int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            print_timestamped("Client disconnected.");
            remove_client(client_socket);
            close(client_socket);
            client_connected = 0;
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
                continue;
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
                close(client_socket);
                return NULL;
            } else {
                send_message(client_socket, "Welcome to the chatroom! Type /help for commands.\n");
                client_connected = 1;
                continue;
            }
        }

        // Command handling
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
                    "/join <room> [limit] - Join or create a room\n"
                    "/leave - Leave current room\n"
                    "/rooms - List all rooms\n"
                    "/react <msg_id> <emoji> - React to a message\n"
                    "/reply <msg_id> <message> - Reply to a message\n");

            } else if (strcmp(buffer, "/quit") == 0) {
                print_timestamped("Client requested disconnect.");
                send_message(client_socket, "Goodbye!\n");
                remove_client(client_socket);
                close(client_socket);
                client_connected = 0;
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
                    send_message(client_socket, "Usage: /sendfile <filename> <filesize>\n");
                    continue;
                }

                send_message(client_socket, "Ready to receive file data\n");

                int fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
                if (fd < 0) {
                    send_message(client_socket, "Failed to open file\n");
                    continue;
                }

                long received = 0;
                while (received < filesize) {
                    int to_read = (filesize - received > BUFFER_SIZE) ? BUFFER_SIZE : (filesize - received);
                    int n = recv(client_socket, buffer, to_read, 0);
                    if (n <= 0) {
                        print_timestamped("File transfer interrupted.");
                        break;
                    }
                    write(fd, buffer, n);
                    received += n;
                }
                close(fd);

                if (received == filesize) {
                    send_message(client_socket, "File transfer complete!\n");
                }

            } else if (strncmp(buffer, "/join ", 6) == 0) {
                char room_name[ROOM_NAME_MAX];
                int limit = ROOM_USER_LIMIT;
                int args = sscanf(buffer + 6, "%31s %d", room_name, &limit);
                if (args < 1) {
                    send_message(client_socket, "Usage: /join <room_name> [limit]\n");
                } else {
                    int result = join_room(client_socket, room_name, limit);
                    if (result == 0) {
                        char join_msg[BUFFER_SIZE];
                        snprintf(join_msg, sizeof(join_msg), "You have joined room '%s'.\n", room_name);
                        send_message(client_socket, join_msg);
                    } else if (result == -1) {
                        send_message(client_socket, "Room list full. Cannot join or create room.\n");
                    } else if (result == -2) {
                        send_message(client_socket, "Room is full.\n");
                    }
                }

            } else if (strcmp(buffer, "/leave") == 0) {
                int idx = find_client_index(client_socket);
                if (idx != -1 && clients[idx].current_room[0] != '\0') {
                    char room_name[ROOM_NAME_MAX];
                    strncpy(room_name, clients[idx].current_room, ROOM_NAME_MAX);
                    leave_room(client_socket, room_name);
                    send_message(client_socket, "You have left the room.\n");
                } else {
                    send_message(client_socket, "You're not in any room.\n");
                }

            } else if (strcmp(buffer, "/rooms") == 0) {
                list_rooms(client_socket);

            } else if (strncmp(buffer, "/react ", 7) == 0) {
                int msg_id;
                char emoji[EMOJI_MAX];
                if (sscanf(buffer + 7, "%d %7s", &msg_id, emoji) == 2) {
                    int idx = find_client_index(client_socket);
                    int room_idx = find_room_index(clients[idx].current_room);
                    if (room_idx != -1) {
                        pthread_mutex_lock(&rooms_mutex);
                        Room_t *room = &rooms[room_idx];
                        if (msg_id > 0 && msg_id <= room->message_count) {
                            ChatMessage *msg = &room->messages[msg_id - 1];
                            int found = 0;
                            for (int r = 0; r < msg->reaction_count; r++) {
                                if (strcmp(msg->reactions[r].emoji, emoji) == 0) {
                                    msg->reactions[r].count++;
                                    found = 1;
                                    break;
                                }
                            }
                            if (!found && msg->reaction_count < MAX_REACTIONS) {
                                strncpy(msg->reactions[msg->reaction_count].emoji, emoji, EMOJI_MAX - 1);
                                msg->reactions[msg->reaction_count].count = 1;
                                msg->reaction_count++;
                            }
                            char rmsg[BUFFER_SIZE];
                            snprintf(rmsg, sizeof(rmsg), "[%s] reacted to message #%d with %s\n",
                                     clients[idx].username, msg_id, emoji);
                            broadcast_to_room(room->name, rmsg);
                        } else {
                            send_message(client_socket, "Invalid message ID.\n");
                        }
                        pthread_mutex_unlock(&rooms_mutex);
                    } else {
                        send_message(client_socket, "You are not in a room.\n");
                    }
                } else {
                    send_message(client_socket, "Usage: /react <msg_id> <emoji>\n");
                }

            } else if (strncmp(buffer, "/reply ", 7) == 0) {
                int msg_id;
                char reply_text[BUFFER_SIZE];
                if (sscanf(buffer + 7, "%d %[^\n]", &msg_id, reply_text) == 2) {
                    int idx = find_client_index(client_socket);
                    int room_idx = find_room_index(clients[idx].current_room);
                    if (room_idx != -1) {
                        pthread_mutex_lock(&rooms_mutex);
                        Room_t *room = &rooms[room_idx];
                        if (msg_id > 0 && msg_id <= room->message_count) {
                            ChatMessage *orig = &room->messages[msg_id - 1];
                            char reply_msg[BUFFER_SIZE * 2];
                            snprintf(reply_msg, sizeof(reply_msg),
                                     "[Reply to #%d from %s: \"%s\"]\n%s [%s]: %s\n",
                                     orig->id, orig->sender, orig->text,
                                     clients[idx].username, clients[idx].status, reply_text);

                            if (room->message_count < MAX_MESSAGES) {
                                ChatMessage *new_msg = &room->messages[room->message_count++];
                                new_msg->id = room->message_count;
                                strncpy(new_msg->sender, clients[idx].username, USERNAME_MAX - 1);
                                strncpy(new_msg->text, reply_text, BUFFER_SIZE - 1);
                                get_current_timestamp(new_msg->timestamp, sizeof(new_msg->timestamp));
                                new_msg->reaction_count = 0;
                            }

                            broadcast_to_room(room->name, reply_msg);
                        } else {
                            send_message(client_socket, "Invalid message ID.\n");
                        }
                        pthread_mutex_unlock(&rooms_mutex);
                    } else {
                        send_message(client_socket, "You are not in a room.\n");
                    }
                } else {
                    send_message(client_socket, "Usage: /reply <msg_id> <message>\n");
                }

            } else {
                send_message(client_socket, "Unknown command. Type /help for commands.\n");
            }

            continue;
        }

        // Normal message, send to room if user is in one
        int idx = find_client_index(client_socket);
        if (idx != -1 && clients[idx].current_room[0] != '\0') {
            int room_idx = find_room_index(clients[idx].current_room);
            if (room_idx != -1) {
                pthread_mutex_lock(&rooms_mutex);
                Room_t *room = &rooms[room_idx];

                if (room->message_count < MAX_MESSAGES) {
                    ChatMessage *msg = &room->messages[room->message_count++];
                    msg->id = room->message_count;
                    strncpy(msg->sender, clients[idx].username, USERNAME_MAX - 1);
                    strncpy(msg->text, buffer, BUFFER_SIZE - 1);
                    get_current_timestamp(msg->timestamp, sizeof(msg->timestamp));
                    msg->reaction_count = 0;

                    char chat_msg[BUFFER_SIZE * 2];
                    snprintf(chat_msg, sizeof(chat_msg), "[%d] %s [%s]: %s\n",
                             msg->id, msg->sender, clients[idx].status, msg->text);
                    print_timestamped(chat_msg);
                    broadcast_to_room(room->name, chat_msg);
                }

                pthread_mutex_unlock(&rooms_mutex);
            }
        } else {
            send_message(client_socket, "Join a room to send messages. Use /join <room>\n");
        }
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
    init_rooms();
    start_server();
    return 0;
}

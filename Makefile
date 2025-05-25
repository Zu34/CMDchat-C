CC = gcc
CFLAGS = -Wall -Wextra -pthread -g
BUILD_DIR = build

TARGETS = server client
SERVER_SRC = server.c
CLIENT_SRC = client.c
COMMON_HDR = common.h

# Make sure build directory exists
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

all: $(BUILD_DIR) $(TARGETS)

$(BUILD_DIR)/server: $(SERVER_SRC) $(COMMON_HDR) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(SERVER_SRC)

$(BUILD_DIR)/client: $(CLIENT_SRC) $(COMMON_HDR) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(CLIENT_SRC)

server: $(BUILD_DIR)/server

client: $(BUILD_DIR)/client

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean server client

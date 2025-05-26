#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "common.h"
#include "utils.h"

// Prints a message with a timestamp prefix
void print_timestamped(const char *msg) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if (t == NULL) {
        perror("localtime");
        return;
    }
    char timebuf[20];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", t);
    printf("[%s] %s\n", timebuf, msg);
}

// Removes trailing newline from a string if present
void trim_newline(char *str) {
    size_t len = strlen(str);
    if (len == 0) return;
    if (str[len - 1] == '\n') {
        str[len - 1] = '\0';
    }
}

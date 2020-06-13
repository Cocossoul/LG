#include "network.h"
#include <unistd.h>
#include <stdlib.h>
#include <err.h>
#include <netdb.h>

#define BUFFER_SIZE 512

//Sending and getting
void write_fd(int fd_out, char* buf, ssize_t to_write, int more) {
    ssize_t offset = 0;
    while (to_write > 0) {
        ssize_t w;
        if (more)
            w = send(fd_out, buf + offset, to_write, MSG_MORE);
        else
            w = write(fd_out, buf + offset, to_write);
        if (w == -1)
            err(EXIT_FAILURE, "write()");
        offset += w;

        to_write -= w;
    }
}

GString * read_from_file(int fd_in) {
    ssize_t r = -1;
    GString * str = g_string_new(NULL);
    while (r != 0) {
        char buf[BUFFER_SIZE];
        buf[BUFFER_SIZE - 1] = 0;
        r = read(fd_in, buf, BUFFER_SIZE-1);
        if (r == -1)
            err(EXIT_FAILURE, "read()");
        g_string_append_len(str, buf, r);
    }
    return str;
}

GString * read_from_socket(int fd_in) {
    ssize_t r = 0;
    GString * str = g_string_new(NULL);
    while (!g_str_has_suffix(str->str, "\r\n\r\n")) {
        char buf[BUFFER_SIZE];
        buf[BUFFER_SIZE - 1] = 0;
        r = read(fd_in, buf, BUFFER_SIZE - 1);
        if (r == 0) {
            g_string_free(str, TRUE);
            return NULL;
        }
        if (r == -1)
            err(EXIT_FAILURE, "read()");
        g_string_append_len(str, buf, r);
    }

    return str;
}

#ifndef NETWORK_H

#define NETWORK_H
#include <glib.h>

void write_fd(int fd_out, char* buf, ssize_t to_write, int more);
GString * read_from_file(int fd_in);
GString * read_from_socket(int fd_in);

#endif

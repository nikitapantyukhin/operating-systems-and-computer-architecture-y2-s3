#pragma once

#include <stddef.h>

int write_all(int fd, const char *buffer, size_t size);
int close_fd(int fd);

#pragma once
#include <stdio.h>

#define log_info(fmt, ...)  printf("[*] " fmt "\n", ##__VA_ARGS__)
#define log_error(fmt, ...) fprintf(stderr, "[!] " fmt "\n", ##__VA_ARGS__)

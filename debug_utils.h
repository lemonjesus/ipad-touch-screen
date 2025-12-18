#include <stdio.h>
#include <stdint.h>

#ifndef DEBUG_UTILS_H
#define DEBUG_UTILS_H

#define DEBUG 0
#define debugf(...) do { if (DEBUG) printf(__VA_ARGS__); } while (0)

#endif

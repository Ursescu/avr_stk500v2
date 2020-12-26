#ifndef _UTIL_H_
#define _UTIL_H

#include <stdint.h>

#define TRUE (1)
#define FALSE (0)

typedef uint8_t bool;

#define PRIVATE static
#define PUBLIC extern

#define LEN(arr) (sizeof((arr)) / sizeof((arr)[0]))

#endif
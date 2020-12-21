#ifndef _DBG_H_
#define _DBG_H_


#include <avr/io.h>
#include "../util.h"

PUBLIC bool initDBG(void);

PUBLIC int printfDBG(const char *__fmt, ...);

#endif
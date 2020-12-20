#ifndef _STK500_H_
#define _STK500_H_

#include <avr/io.h>
#include "../util.h"


/* CB needed to push the data */
typedef bool (*userCbFunc_t)(uint8_t);

/* Init STK500 */
PUBLIC bool initSTK500(userCbFunc_t func);

/* Will fill the command buffer */
PUBLIC bool processByteSTK500(uint8_t byte);

/* Handle command */
PUBLIC bool handleCommandSTK500(void);



#endif
#ifndef _SPI_H_
#define _SPI_H_

#include "util.h"

bool initSPI(uint8_t mode, uint8_t clock);

bool sendByteSPI(uint8_t byte);
bool getByteSPI(uint8_t *byte);

bool triggerSendSPI(void);
bool triggerReceiveSPI(uint32_t numBytes);

#endif
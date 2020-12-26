#ifndef _SPI_H_
#define _SPI_H_

#include <util.h>

#define SPI_BUFFER_SIZE 100

PUBLIC bool initSPI(uint8_t mode, uint8_t clock);

PUBLIC bool sendSpiByte(uint8_t byte);
PUBLIC bool getSpiByte(uint8_t *byte);

/* In case need some bytes without explicit send */
PUBLIC bool sendSpiReceive(uint32_t numBytes);

#endif
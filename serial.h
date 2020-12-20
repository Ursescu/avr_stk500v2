#ifndef _SERIAL_H_
#define _SERIAL_H_

#include <avr/io.h>

uint8_t initSerial(uint8_t ucDataBits, uint8_t eParity);
uint8_t serialGetByte(uint8_t *pucByte);
uint8_t serialPutByte(uint8_t ucByte);
void enableSerial(uint8_t xRxEnable, uint8_t xTxEnable);

void serialTransmitCb(void);
void serialReceiveCb(void);

#endif
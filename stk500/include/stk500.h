#ifndef _STK500_H_
#define _STK500_H_

#include <stdint.h>
#include <util.h>

/* UART functions to get/push data from/to the user */
typedef bool (*uartSendByteFunc_t)(uint8_t);
typedef bool (*uartGetByteFunc_t)(uint8_t *);

/* SPI functions to get/push data from/to the target AVR */
typedef bool (*spiSendByteFunc_t)(uint8_t);
typedef bool (*spiGetByteFunc_t)(uint8_t *);

/* User code to reset target */
typedef void (*resetTargetFunc_t)(void);

/* Init STK500 */
PUBLIC bool initSTK500(uartSendByteFunc_t userSend,
                       uartGetByteFunc_t userGet,
                       spiSendByteFunc_t targetSend,
                       spiGetByteFunc_t targetGet,
                       resetTargetFunc_t targetReset);

/* The state machine that should be called */
PUBLIC void tickSTK500(void);

#endif
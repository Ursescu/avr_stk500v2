#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#include <avr/io.h>

#include "options.h"
#include "command.h"

typedef struct STK500_command {
    uint8_t start;
    uint8_t seq;
    uint16_t size;
    uint8_t token;
    uint8_t checksum;
    uint8_t body[STK500_MAX_SIZE];
} STK500_command_t;

typedef enum {
    START = 0x00,
    SEQ,
    SIZE,
    TOKEN,
    BODY,
    CHECKSUM
} STK500_command_states;

typedef enum {
    IDLE = 0x00,
    PROCESSING,
    PROCESSED,
    HANDLING,
} STK500_fsm_states;

typedef struct STK500_param {
    uint8_t id;
    uint8_t value;
} STK500_param_t;

#endif
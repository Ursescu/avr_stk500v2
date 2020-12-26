#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#include <stdint.h>

#include "options.h"
#include "command.h"

#define STK500_ISP_COMMAND_LEN 4U
#define STK500_ISP_COMMAND_VALUE_INDEX 3U

#define STK500_FLASH_MODE_BIT(mode) ((mode)&0x01)

// Flash mode bit getters
#define STK500_FLASH_WORD_MODE_TIME_BIT(mode) (((mode) >> 1) & 0x01)
#define STK500_FLASH_WORD_MODE_POLL_BIT(mode) (((mode) >> 2) & 0x01)
#define STK500_FLASH_WORD_MODE_RDY_BIT(mode) (((mode) >> 3) & 0x01)

#define STK500_FLASH_PAGE_MODE_TIME_BIT(mode) (((mode) >> 4) & 0x01)
#define STK500_FLASH_PAGE_MODE_POLL_BIT(mode) (((mode) >> 5) & 0x01)
#define STK500_FLASH_PAGE_MODE_RDY_BIT(mode) (((mode) >> 6) & 0x01)
#define STK500_FLASH_PAGE_MODE_WRITE_BIT(mode) (((mode) >> 7) & 0x01)

#define STK500_FLASH_HIGH_BYTE_BIT (3)
#define STK500_FLASH_HIGH_BYTE_CMD(cmd) ((cmd) | (1 << STK500_FLASH_HIGH_BYTE_BIT))
#define STK500_FLASH_LOW_BYTE_CMD(cmd) ((cmd) & ~(1 << STK500_FLASH_HIGH_BYTE_BIT))

typedef struct STK500_command
{
    uint8_t start;
    uint8_t seq;
    uint16_t size;
    uint8_t token;
    uint8_t checksum;
    uint8_t body[STK500_MAX_SIZE];
} STK500_command_t;

typedef enum
{
    START = 0x00,
    SEQ,
    SIZE,
    TOKEN,
    BODY,
    CHECKSUM
} STK500_command_states;

typedef enum
{
    IDLE = 0x00,
    PROCESSING,
    PROCESSED,
    HANDLING,
} STK500_fsm_states;

typedef struct STK500_param
{
    uint8_t id;
    uint8_t value;
} STK500_param_t;

typedef struct STK500_settings
{
    bool inProgMode;
    uint8_t timeout;
    uint32_t loadAddress;
} STK500_settings_t;

#endif

#include <avr/io.h>
#include <string.h>

#include "stk500.h"
#include "protocol.h"
#include "command.h"
#include "misc.h"
#include "../util.h"

PRIVATE STK500_command_t command;
PRIVATE STK500_command_t response;
PRIVATE STK500_fsm_states fsm = IDLE;

/* User callback to push data */
PRIVATE userCbFunc_t userCb;

/* Param table for stk500 programmer */
PRIVATE STK500_param_t params[] = {
    {PARAM_BUILD_NUMBER_LOW, 0U},
    {PARAM_BUILD_NUMBER_HIGH, 0U},
    {PARAM_HW_VER, 1U},
    {PARAM_SW_MAJOR, 0U},
    {PARAM_SW_MINOR, 1U},
    {PARAM_VTARGET, 0U},
    {PARAM_VADJUST, 0U},
    {PARAM_OSC_PSCALE, 0U},
    {PARAM_OSC_CMATCH, 0U},
    {PARAM_SCK_DURATION, 0U},
    {PARAM_TOPCARD_DETECT, 0U},
    {PARAM_STATUS, 0U},
    {PARAM_DATA, 0U},
    {PARAM_RESET_POLARITY, 0U},
    {PARAM_CONTROLLER_INIT, 0U},
};

PRIVATE uint8_t checksum(STK500_command_t *command)
{
    uint8_t xor = 0U;

    xor ^= command->start;
    xor ^= command->seq;
    xor ^= (uint8_t)(command->size >> 8);
    xor ^= (uint8_t)(command->size);
    xor ^= command->token;

    for (uint32_t index = 0; index < STK500_MAX_SIZE; index++)
    {
        xor ^= command->body[index];
    }

    return xor;
}

PRIVATE bool getParamVal(uint8_t id, uint8_t *val)
{
    for (uint8_t index = 0; index < LEN(params); index++)
    {
        if (params[index].id == id)
        {
            *val = params[index].value;
            return TRUE;
        }
    }

    return FALSE;
}

PRIVATE bool setParamVal(uint8_t id, uint8_t val)
{
    for (uint8_t index = 0; index < LEN(params); index++)
    {
        if (params[index].id == id)
        {
            params[index].value = val;
            return TRUE;
        }
    }

    return FALSE;
}

PRIVATE bool pushResponse(void)
{
    /* Push bytes to user - Order matters */
    userCb(response.start);
    userCb(response.seq);
    userCb((uint8_t)(response.size >> 8));
    userCb(((uint8_t)(response.size)));
    userCb(response.token);
    for (uint32_t index = 0; index < response.size; index++)
    {
        userCb(response.body[index]);
    }
    userCb(response.checksum);

    return TRUE;
}

PUBLIC bool initSTK500(userCbFunc_t cb)
{
    userCb = cb;

    return TRUE;
}

PUBLIC bool processByteSTK500(uint8_t byte)
{
    static STK500_command_states state = START;
    static uint32_t requested = 0U;

    bool error = FALSE;

    switch (state)
    {
    case START:
        command.start = byte;
        state = SEQ;
        fsm = PROCESSING;
        if (command.start != MESSAGE_START)
        {
            error = TRUE;
        }
        break;
    case SEQ:
        command.seq = byte;
        state = SIZE;
        requested = 2U;
        break;
    case SIZE:
    {
        switch (requested)
        {
        case 2U:
            command.size = (byte << 8);
            requested--;
            break;
        default:
            command.size = (command.size & 0xff00) | byte;
            requested = 0U;
            state = TOKEN;
            break;
        }
        break;
    }
    case TOKEN:
    {
        command.token = byte;
        state = BODY;
        requested = command.size;
        if (requested > STK500_MAX_SIZE || command.token != MESSAGE_TOKEN)
        {
            error = TRUE;
        }
        break;
    }

    case BODY:
    {
        if (requested > 0U)
        {
            command.body[command.size - requested] = byte;
            requested--;
        }

        if (requested == 0U)
        {
            state = CHECKSUM;
        }

        break;
    }
    case CHECKSUM:
    {
        uint8_t calcChecksum;
        command.checksum = byte;
        calcChecksum = checksum(&command);
        if (command.checksum != calcChecksum)
        {
            error = TRUE;
        }

        state = START;
        fsm = PROCESSED;
        break;
    }
    default:
        /* Unhandled state */
        error = TRUE;
    }

    if (error)
    {
        // Reset the state machine
        state = START;
        requested = 0U;
        fsm = IDLE;
        return FALSE;
    }

    return TRUE;
}

PUBLIC bool handleCommandSTK500()
{
    uint8_t *commandBuffer, *resp;
    uint8_t commandId;
    bool error = FALSE;
    uint32_t size = 0U;

    if (fsm != PROCESSED)
    {
        // We cant handle command because it's not yet processed
        return FALSE;
    }

    fsm = HANDLING;

    /* Prepare response structure */
    response.start = MESSAGE_START;
    response.token = MESSAGE_TOKEN;
    response.seq = command.seq;
    response.size = 0U;

    memset(response.body, 0U, STK500_MAX_SIZE);

    /* Handle STK500 command */
    commandBuffer = command.body;
    commandId = commandBuffer[0];
    resp = response.body;

    switch (commandId)
    {
    case CMD_SIGN_ON:
    {
        WRITE_BUF8(resp, CMD_SIGN_ON, size);
        WRITE_BUF8(resp, STATUS_CMD_OK, size);
        WRITE_BUF8(resp, 8U, size);
        memcpy(resp, STK500_SIGNATURE, sizeof(STK500_SIGNATURE) - 1);
        size += sizeof(STK500_SIGNATURE) - 1;
        break;
    }

    case CMD_SET_PARAMETER:
    {
        uint8_t paramId;
        uint8_t paramVal;
        uint8_t status;

        paramId = commandBuffer[1];
        paramVal = commandBuffer[2];

        status = setParamVal(paramId, paramVal);

        WRITE_BUF8(resp, CMD_SET_PARAMETER, size);
        WRITE_BUF8(resp, ((status == TRUE) ? STATUS_CMD_OK : STATUS_CMD_FAILED), size);
        break;
    }

    case CMD_GET_PARAMETER:
    {
        uint8_t paramId;
        uint8_t paramVal;
        uint8_t status;

        paramId = commandBuffer[1];
        status = getParamVal(paramId, &paramVal);
        WRITE_BUF8(resp, CMD_GET_PARAMETER, size);

        if (status)
        {
            WRITE_BUF8(resp, STATUS_CMD_OK, size);
            WRITE_BUF8(resp, paramVal, size);
        }
        else
        {
            WRITE_BUF8(resp, STATUS_CMD_FAILED, size);
        }
        break;
    }
    case CMD_ENTER_PROGMODE_ISP:
    {
        WRITE_BUF8(resp, CMD_ENTER_PROGMODE_ISP, size);
        WRITE_BUF8(resp, STATUS_CMD_OK, size);

        break;
    }

    case CMD_LEAVE_PROGMODE_ISP:
    {
        WRITE_BUF8(resp, CMD_LEAVE_PROGMODE_ISP, size);
        WRITE_BUF8(resp, STATUS_CMD_OK, size);

        break;
    }
    default:
        /* Unhandleded command */
        error = TRUE;
        break;
    }

    if (error)
    {
        fsm = IDLE;
        return FALSE;
    }

    /* Set length */
    response.size = size;

    /* Calculate checksum */
    response.checksum = checksum(&response);

    pushResponse();

    fsm = IDLE;

    return TRUE;
}

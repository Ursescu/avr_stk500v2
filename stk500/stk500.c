#include <avr/io.h>
#include <string.h>
#include <stdint.h>
#include <util/delay.h>

#include <dbg.h>
#include <util.h>
#include <stk500.h>
#include <timer.h>

#include "protocol.h"
#include "command.h"
#include "misc.h"

PRIVATE STK500_command_t command;
PRIVATE STK500_command_t response;

PRIVATE STK500_fsm_states fsm = IDLE;

PRIVATE STK500_settings_t settings;

/* User callback to push data */
PRIVATE uartSendByteFunc_t userSend;
PRIVATE uartGetByteFunc_t userGet;

/* Target callbacks to push/get data */
PRIVATE spiSendByteFunc_t targetSend;
PRIVATE spiGetByteFunc_t targetGet;

/* Target reset */
PRIVATE resetTargetFunc_t targetReset;

/* Param table for stk500 programmer */
PRIVATE STK500_param_t params[] = {
    {PARAM_BUILD_NUMBER_LOW, 0U},
    {PARAM_BUILD_NUMBER_HIGH, 0U},
    {PARAM_HW_VER, 0U},
    {PARAM_SW_MAJOR, 0U},
    {PARAM_SW_MINOR, 0U},
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

PRIVATE uint8_t checksum(STK500_command_t *command) {
    uint8_t xor = 0U;

    xor ^= command->start;
    xor ^= command->seq;
    xor ^= (uint8_t)(command->size >> 8);
    xor ^= (uint8_t)(command->size);
    xor ^= command->token;

    for (uint32_t index = 0; index < command->size; index++) {
        xor ^= command->body[index];
    }

    return xor;
}

PRIVATE bool getParamVal(uint8_t id, uint8_t *val) {
    for (uint8_t index = 0; index < LEN(params); index++) {
        if (params[index].id == id) {
            *val = params[index].value;
            return TRUE;
        }
    }

    return FALSE;
}

PRIVATE bool setParamVal(uint8_t id, uint8_t val) {
    for (uint8_t index = 0; index < LEN(params); index++) {
        if (params[index].id == id) {
            params[index].value = val;
            return TRUE;
        }
    }

    return FALSE;
}

PRIVATE bool pushResponse(void) {
    /* Push bytes to user - Order matters */

    userSend(response.start);
    userSend(response.seq);
    userSend((uint8_t)(response.size >> 8));
    userSend(((uint8_t)(response.size)));
    userSend(response.token);
    for (uint32_t index = 0; index < response.size; index++) {
        userSend(response.body[index]);
    }
    userSend(response.checksum);

    return TRUE;
}

PRIVATE bool sendCommand(const uint8_t *cmd, uint8_t *out) {
    uint64_t timerCounter;
    uint8_t byteIndex = 0U;
    uint8_t recvByte;
    bool timeouted = FALSE;

    for (uint8_t index = 0; index < STK500_ISP_COMMAND_LEN; index++) {
        targetSend(cmd[index]);
    }

    timerCounter = timer_read_counter();

    /* Should poll for the byte */
    while (byteIndex < STK500_ISP_COMMAND_LEN && !timeouted) {
        if ((timer_read_counter() - timerCounter) >= settings.timeout) {
            timeouted = TRUE;
            break;
        }
        if (targetGet(&recvByte)) {
            (*out++) = recvByte;
            byteIndex++;
        }
    }

    if (timeouted)
        return FALSE;

    return TRUE;
}

PRIVATE bool pollRDY(uint8_t *val) {
    uint8_t cmd[STK500_ISP_COMMAND_LEN] = {0xf0, 0x00, 0x00, 0x00};
    uint8_t out[STK500_ISP_COMMAND_LEN] = {
        0U,
    };

    if (!sendCommand(cmd, out)) {
        return FALSE;
    }

    *val = out[STK500_ISP_COMMAND_VALUE_INDEX];

    return TRUE;
}

PRIVATE bool processByteSTK500(uint8_t byte) {
    static STK500_command_states state = START;
    static uint32_t requested = 0U;

    bool error = FALSE;

    switch (state) {
        case START:
            command.start = byte;
            state = SEQ;
            fsm = PROCESSING;
            if (command.start != MESSAGE_START) {
                error = TRUE;
            }
            break;
        case SEQ:
            command.seq = byte;
            state = SIZE;
            requested = 2U;
            break;
        case SIZE: {
            switch (requested) {
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
        case TOKEN: {
            command.token = byte;
            state = BODY;
            requested = command.size;
            if (requested > STK500_MAX_SIZE || command.token != MESSAGE_TOKEN) {
                error = TRUE;
            }
            break;
        }

        case BODY: {
            if (requested > 0U) {
                command.body[command.size - requested] = byte;
                requested--;
            }

            if (requested == 0U) {
                state = CHECKSUM;
            }

            break;
        }
        case CHECKSUM: {
            uint8_t calcChecksum;
            command.checksum = byte;
            calcChecksum = checksum(&command);
            if (command.checksum != calcChecksum) {
                error = TRUE;
            }
            // PORTD ^= _BV(PD7);
            state = START;
            fsm = PROCESSED;
            break;
        }
        default:
            /* Unhandled state */
            error = TRUE;
    }

    if (error) {
        // Reset the state machine
        printfDBG("Process byte fsm error\n");
        state = START;
        requested = 0U;
        fsm = IDLE;
        return FALSE;
    }

    return TRUE;
}

PRIVATE bool handleCommandSTK500(void) {
    uint8_t *commandBuffer, *resp, *req;
    uint8_t commandId;
    bool error = FALSE;
    uint32_t size = 0U;

    if (fsm != PROCESSED) {
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
    req = &commandBuffer[1];
    resp = response.body;

    switch (commandId) {
        case CMD_SIGN_ON: {
            WRITE_BUF8(resp, CMD_SIGN_ON, size);
            WRITE_BUF8(resp, STATUS_CMD_OK, size);
            WRITE_BUF8(resp, sizeof(STK500_SIGNATURE) - 1, size);
            memcpy(resp, STK500_SIGNATURE, sizeof(STK500_SIGNATURE) - 1);
            size += sizeof(STK500_SIGNATURE) - 1;
            break;
        }
        case CMD_SET_PARAMETER: {
            uint8_t paramId;
            uint8_t paramVal;
            uint8_t status;

            READ_BUF8(req, paramId);
            READ_BUF8(req, paramVal);

            status = setParamVal(paramId, paramVal);

            WRITE_BUF8(resp, CMD_SET_PARAMETER, size);
            WRITE_BUF8(resp, ((status == TRUE) ? STATUS_CMD_OK : STATUS_CMD_FAILED), size);
            break;
        }
        case CMD_GET_PARAMETER: {
            uint8_t paramId;
            uint8_t paramVal;
            uint8_t status;

            READ_BUF8(req, paramId);
            status = getParamVal(paramId, &paramVal);
            WRITE_BUF8(resp, CMD_GET_PARAMETER, size);

            if (status) {
                WRITE_BUF8(resp, STATUS_CMD_OK, size);
                WRITE_BUF8(resp, paramVal, size);
            } else {
                WRITE_BUF8(resp, STATUS_CMD_FAILED, size);
            }
            break;
        }
        case CMD_LOAD_ADDRESS: {
            // Only performing on devices with less than 64k flash
            uint8_t byte;

            if (settings.inProgMode) {
                settings.loadAddress = 0U;
                READ_BUF8(req, byte);
                settings.loadAddress |= ((uint32_t)byte) << 24;

                READ_BUF8(req, byte);
                settings.loadAddress |= ((uint32_t)byte) << 16;

                READ_BUF8(req, byte);
                settings.loadAddress |= ((uint32_t)byte) << 8;

                READ_BUF8(req, byte);
                settings.loadAddress |= byte;
            }

            WRITE_BUF8(resp, CMD_LOAD_ADDRESS, size);
            WRITE_BUF8(resp, STATUS_CMD_OK, size);
            break;
        }
        case CMD_ENTER_PROGMODE_ISP: {
            uint8_t timeout;
            uint8_t stabDelay;
            uint8_t cmdexeDelay;
            uint8_t syncLoops;
            uint8_t byteDelay;
            uint8_t pollValue;
            uint8_t pollIndex;
            uint8_t cmd[STK500_ISP_COMMAND_LEN];
            bool entered = FALSE;
            bool timeouted = FALSE;
            uint64_t timerCounter;

            READ_BUF8(req, timeout);
            READ_BUF8(req, stabDelay);
            READ_BUF8(req, cmdexeDelay);
            READ_BUF8(req, syncLoops);
            READ_BUF8(req, byteDelay);
            READ_BUF8(req, pollValue);
            READ_BUF8(req, pollIndex);

            memcpy(cmd, req, STK500_ISP_COMMAND_LEN);

            settings.inProgMode = FALSE;

            WRITE_BUF8(resp, CMD_ENTER_PROGMODE_ISP, size);

            settings.timeout = timeout;

            for (uint8_t retry = 0; (retry < syncLoops && !entered && !timeouted); retry++) {
                /* Reset target */
                PORTB &= ~(_BV(PB7));
                PORTD |= _BV(PD6);

                delay_ms(stabDelay);

                PORTD &= ~(_BV(PD6));

                for (uint8_t index = 0; index < STK500_ISP_COMMAND_LEN; index++) {
                    targetSend(cmd[index]);
                    delay_ms(byteDelay);
                }

                delay_ms(cmdexeDelay);

                timerCounter = timer_read_counter();

                if (pollIndex != 0U) {
                    /* Should poll for the byte */
                    uint8_t byteIndex = 0U;
                    uint8_t recvByte;

                    while (byteIndex < STK500_ISP_COMMAND_LEN) {
                        if ((timer_read_counter() - timerCounter) >= settings.timeout) {
                            // We have a timeout here
                            timeouted = TRUE;
                            break;
                        }
                        if (targetGet(&recvByte)) {
                            byteIndex++;
                            if (byteIndex == pollIndex && recvByte == pollValue) {
                                entered = TRUE;
                            }
                        }
                    }
                }
            }

            if (timeouted) {
                printfDBG("Timeout %d %d %d %d\n", stabDelay, cmdexeDelay, byteDelay, settings.timeout);
                WRITE_BUF8(resp, STATUS_CMD_TOUT, size);
            } else if (entered) {
                settings.inProgMode = TRUE;
                WRITE_BUF8(resp, STATUS_CMD_OK, size);
            } else {
                WRITE_BUF8(resp, STATUS_CMD_FAILED, size);
            }

            break;
        }
        case CMD_LEAVE_PROGMODE_ISP: {
            uint8_t preDelay;
            uint8_t postDelay;

            READ_BUF8(req, preDelay);
            READ_BUF8(req, postDelay);

            if (settings.inProgMode) {
                settings.inProgMode = FALSE;
                delay_ms(preDelay);

                PORTD |= _BV(PD6);

                delay_ms(postDelay);
            }

            WRITE_BUF8(resp, CMD_LEAVE_PROGMODE_ISP, size);
            WRITE_BUF8(resp, STATUS_CMD_OK, size);

            break;
        }
        case CMD_CHIP_ERASE_ISP: {
            uint8_t eraseDelay;
            uint8_t pollMethod;
            uint8_t cmd[STK500_ISP_COMMAND_LEN];
            uint8_t out[STK500_ISP_COMMAND_LEN];
            bool error = FALSE;
            uint64_t timerCounter = timer_read_counter();

            READ_BUF8(req, eraseDelay);
            READ_BUF8(req, pollMethod);
            memcpy(cmd, req, STK500_ISP_COMMAND_LEN);

            WRITE_BUF8(resp, CMD_CHIP_ERASE_ISP, size);

            if (!sendCommand(cmd, out)) {
                error = TRUE;
            }
            if (!error) {
                if (pollMethod == 0U) {
                    delay_ms(eraseDelay);
                } else {
                    uint8_t pollingValue = 1U;
                    while (pollingValue && !error) {
                        if (timer_read_counter() - timerCounter >= settings.timeout) {
                            error = TRUE;
                            break;
                        }
                        if (!pollRDY(&pollingValue)) {
                            // Error timeout
                            error = TRUE;
                            break;
                        }
                    }
                }
            }
            WRITE_BUF8(resp, (error == TRUE) ? STATUS_CMD_TOUT : STATUS_CMD_OK, size);

            break;
        }
        case CMD_PROGRAM_FLASH_ISP: {
            uint16_t numBytes = 0U;
            uint8_t mode;
            uint8_t delay;
            uint8_t cmd1, cmd2, cmd3;
            uint8_t poll1, poll2;
            uint8_t dataByte;
            uint64_t timerCounter;
            uint8_t loadCmd[STK500_ISP_COMMAND_LEN] = {
                0U,
            };
            uint8_t writeCmd[STK500_ISP_COMMAND_LEN] = {
                0U,
            };
            uint8_t readCmd[STK500_ISP_COMMAND_LEN] = {
                0U,
            };
            uint8_t output[STK500_ISP_COMMAND_LEN] = {
                0U,
            };
            uint32_t copyAddress;
            bool error = FALSE;

            READ_BUF8(req, dataByte);
            numBytes |= ((uint16_t)numBytes << 8) & 0xff00;
            READ_BUF8(req, dataByte);
            numBytes |= dataByte;

            READ_BUF8(req, mode);
            READ_BUF8(req, delay);
            READ_BUF8(req, cmd1);
            READ_BUF8(req, cmd2);
            READ_BUF8(req, cmd3);
            READ_BUF8(req, poll1);
            READ_BUF8(req, poll2);

            WRITE_BUF8(resp, CMD_PROGRAM_FLASH_ISP, size);

            // Program
            if (STK500_FLASH_MODE_BIT(mode)) {
                // Page mode

                writeCmd[0] = cmd2;
                writeCmd[1] = (settings.loadAddress >> 8) & 0xff;
                writeCmd[2] = settings.loadAddress & 0xff;

                readCmd[0] = cmd3;

                copyAddress = settings.loadAddress;

                // Load page values
                for (uint16_t index = 0; index < (numBytes / 2) && !error; index++) {
                    // Low byte
                    loadCmd[0] = cmd1;
                    loadCmd[1] = ((copyAddress >> 8) & 0xff);
                    loadCmd[2] = (copyAddress & 0xff);
                    loadCmd[3] = req[index * 2];
                    if (!sendCommand(loadCmd, output)) {
                        error = TRUE;
                        break;
                    }

                    // High byte
                    loadCmd[0] = STK500_FLASH_HIGH_BYTE_CMD(cmd1);
                    // Same address
                    loadCmd[3] = req[index * 2 + 1];
                    if (!sendCommand(loadCmd, output)) {
                        error = TRUE;
                        break;
                    }
                    copyAddress++;
                }

                // Commit page
                if (!error && STK500_FLASH_PAGE_MODE_WRITE_BIT(mode) && !sendCommand(writeCmd, output)) {
                    error = TRUE;
                }

                if (STK500_FLASH_PAGE_MODE_TIME_BIT(mode)) {
                    delay_ms(delay);
                } else if (STK500_FLASH_PAGE_MODE_POLL_BIT(mode)) {
                } else if (STK500_FLASH_PAGE_MODE_RDY_BIT(mode)) {
                } else {
                    error = TRUE;
                }
            }
            {
                // Word mode
            }

            printfDBG("CMD_PROGRAM_FLASH_ISP %hu 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x", numBytes, mode, delay, cmd1, cmd2, cmd3, poll1, poll2);

            if (error) {
                WRITE_BUF8(resp, STATUS_CMD_TOUT, size);
            } else {
                WRITE_BUF8(resp, STATUS_CMD_OK, size);
            }

            break;
        }
        case CMD_READ_FLASH_ISP: {
            uint16_t numBytes = 0U;
            uint8_t dataByte;
            uint8_t cmd1;
            uint8_t readCmd[STK500_ISP_COMMAND_LEN] = {
                0U,
            };
            uint8_t out[STK500_ISP_COMMAND_LEN];
            uint32_t copyAddress;
            bool error = FALSE;

            READ_BUF8(req, dataByte);
            numBytes |= ((uint16_t)numBytes << 8) & 0xff00;
            READ_BUF8(req, dataByte);
            numBytes |= dataByte;

            READ_BUF8(req, cmd1);
            copyAddress = settings.loadAddress;

            WRITE_BUF8(resp, CMD_READ_FLASH_ISP, size);
            WRITE_BUF8(resp, STATUS_CMD_OK, size);

            for (uint16_t index = 0U; index < numBytes / 2 && !error; index++) {
                readCmd[0] = cmd1;
                readCmd[1] = (copyAddress >> 8) & 0xff;
                readCmd[2] = copyAddress & 0xff;
                if (!sendCommand(readCmd, out)) {
                    error = TRUE;
                    break;
                }

                WRITE_BUF8(resp, out[STK500_ISP_COMMAND_VALUE_INDEX], size);
                // printfDBG("Read 0x%02x\n", out[STK500_ISP_COMMAND_VALUE_INDEX]);
                readCmd[0] = STK500_FLASH_HIGH_BYTE_CMD(cmd1);
                // Same address

                if (!sendCommand(readCmd, out)) {
                    error = TRUE;
                    break;
                }
                WRITE_BUF8(resp, out[STK500_ISP_COMMAND_VALUE_INDEX], size);
                // printfDBG("Read 0x%02x\n", out[STK500_ISP_COMMAND_VALUE_INDEX]);

                copyAddress++;
            }

            // In case of one low byte
            if (numBytes % 2) {
                readCmd[0] = cmd1;
                readCmd[1] = (copyAddress >> 8) & 0xff;
                readCmd[2] = copyAddress & 0xff;
                if (!sendCommand(readCmd, out)) {
                    error = TRUE;
                    break;
                }

                WRITE_BUF8(resp, out[STK500_ISP_COMMAND_VALUE_INDEX], size);
            }
            if (!error) {
                WRITE_BUF8(resp, STATUS_CMD_OK, size);
            } else {
                // Overwrite hack
                response.body[1] = STATUS_CMD_FAILED;
                size = 2;
            }

            break;
        }
        case CMD_READ_OSCCAL_ISP: {
            uint8_t retAddr;
            uint8_t cmd[STK500_ISP_COMMAND_LEN];
            uint8_t recvByte;
            uint8_t byteIndex = 0U;

            READ_BUF8(req, retAddr);

            memcpy(cmd, req, STK500_ISP_COMMAND_LEN);

            WRITE_BUF8(resp, CMD_READ_OSCCAL_ISP, size);
            WRITE_BUF8(resp, STATUS_CMD_OK, size);

            for (uint8_t index = 0; index < STK500_ISP_COMMAND_LEN; index++) {
                targetSend(cmd[index]);
            }

            /* Should poll for the byte */
            while (byteIndex < STK500_ISP_COMMAND_LEN) {
                if (targetGet(&recvByte)) {
                    byteIndex++;
                    if (byteIndex == retAddr) {
                        WRITE_BUF8(resp, recvByte, size);
                    }
                }
            }
            WRITE_BUF8(resp, STATUS_CMD_OK, size);

            break;
        }
        case CMD_PROGRAM_LOCK_ISP: {
            uint8_t cmd[STK500_ISP_COMMAND_LEN];
            uint8_t out[STK500_ISP_COMMAND_LEN];

            memcpy(cmd, req, STK500_ISP_COMMAND_LEN);

            (void)sendCommand(cmd, out);

            WRITE_BUF8(resp, CMD_PROGRAM_LOCK_ISP, size);
            WRITE_BUF8(resp, STATUS_CMD_OK, size);
            WRITE_BUF8(resp, STATUS_CMD_OK, size);

            break;
        }
        case CMD_READ_LOCK_ISP: {
            uint8_t retAddr;
            uint8_t cmd[STK500_ISP_COMMAND_LEN];
            uint8_t recvByte;
            uint8_t byteIndex = 0U;

            READ_BUF8(req, retAddr);

            memcpy(cmd, req, STK500_ISP_COMMAND_LEN);

            WRITE_BUF8(resp, CMD_READ_LOCK_ISP, size);
            WRITE_BUF8(resp, STATUS_CMD_OK, size);

            for (uint8_t index = 0; index < STK500_ISP_COMMAND_LEN; index++) {
                targetSend(cmd[index]);
            }

            /* Should poll for the byte */
            while (byteIndex < STK500_ISP_COMMAND_LEN) {
                if (targetGet(&recvByte)) {
                    byteIndex++;
                    if (byteIndex == retAddr) {
                        WRITE_BUF8(resp, recvByte, size);
                    }
                }
            }
            WRITE_BUF8(resp, STATUS_CMD_OK, size);

            break;
        }
        case CMD_PROGRAM_FUSE_ISP: {
            uint8_t cmd[STK500_ISP_COMMAND_LEN];
            uint8_t out[STK500_ISP_COMMAND_LEN];

            memcpy(cmd, req, STK500_ISP_COMMAND_LEN);

            (void)sendCommand(cmd, out);

            WRITE_BUF8(resp, CMD_PROGRAM_FUSE_ISP, size);
            WRITE_BUF8(resp, STATUS_CMD_OK, size);
            WRITE_BUF8(resp, STATUS_CMD_OK, size);

            break;
        }
        case CMD_READ_FUSE_ISP: {
            uint8_t retAddr;
            uint8_t cmd[STK500_ISP_COMMAND_LEN];
            uint8_t recvByte;
            uint8_t byteIndex = 0U;

            READ_BUF8(req, retAddr);

            memcpy(cmd, req, STK500_ISP_COMMAND_LEN);

            WRITE_BUF8(resp, CMD_READ_FUSE_ISP, size);
            WRITE_BUF8(resp, STATUS_CMD_OK, size);

            for (uint8_t index = 0; index < STK500_ISP_COMMAND_LEN; index++) {
                targetSend(cmd[index]);
            }

            /* Should poll for the byte */
            while (byteIndex < STK500_ISP_COMMAND_LEN) {
                if (targetGet(&recvByte)) {
                    byteIndex++;
                    if (byteIndex == retAddr) {
                        WRITE_BUF8(resp, recvByte, size);
                    }
                }
            }
            WRITE_BUF8(resp, STATUS_CMD_OK, size);

            break;
        }
        case CMD_READ_SIGNATURE_ISP: {
            uint8_t retAddr;
            uint8_t cmd[STK500_ISP_COMMAND_LEN];
            uint8_t recvByte;
            uint8_t byteIndex = 0U;

            READ_BUF8(req, retAddr);

            memcpy(cmd, req, STK500_ISP_COMMAND_LEN);

            WRITE_BUF8(resp, CMD_READ_SIGNATURE_ISP, size);
            WRITE_BUF8(resp, STATUS_CMD_OK, size);

            for (uint8_t index = 0; index < STK500_ISP_COMMAND_LEN; index++) {
                targetSend(cmd[index]);
            }

            /* Should poll for the byte */
            while (byteIndex < STK500_ISP_COMMAND_LEN) {
                if (targetGet(&recvByte)) {
                    byteIndex++;
                    if (byteIndex == retAddr) {
                        WRITE_BUF8(resp, recvByte, size);
                    }
                }
            }
            WRITE_BUF8(resp, STATUS_CMD_OK, size);
            break;
        }
        default:
            /* Unhandleded command */
            error = TRUE;

            break;
    }

    if (error) {
        printfDBG("Process command fsm error\n");
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

PUBLIC void tickSTK500(void) {
    uint8_t byte;

    if (userGet(&byte)) {
        printfDBG("RCV 0x%02x\n", byte);
        processByteSTK500(byte);
    }

    if (handleCommandSTK500()) {
        printfDBG("Command handled\n");
    }

    /*  Don't stale the in this loop
        userGet should be atomic, meaning that interrupts are disabled, 
        so try not to keep them disabled for too long.
    */
    _delay_ms(1);
}

PUBLIC bool initSTK500(uartSendByteFunc_t uartSend, uartGetByteFunc_t uartGet, spiSendByteFunc_t spiSend, spiGetByteFunc_t spiGet, resetTargetFunc_t reset) {
    userSend = uartSend;
    userGet = uartGet;
    targetSend = spiSend;
    targetGet = spiGet;
    targetReset = reset;

    // Defaults
    settings.inProgMode = FALSE;
    settings.timeout = 0U;
    settings.loadAddress = 0U;

    return TRUE;
}

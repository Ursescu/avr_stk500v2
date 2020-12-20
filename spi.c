#include <avr/io.h>
#include <avr/interrupt.h>

#include "spi.h"
#include "buffer.h"

#define SPI_MOSI PB5
#define SPI_SCK PB7
#define SPI_SS PB4
#define SPI_DDR DDRB
#define SPI_PORT PORTB

#define SPI_BUFFER_SIZE 200

static uint8_t _txBuffer[SPI_BUFFER_SIZE];
static uint8_t _rxBuffer[SPI_BUFFER_SIZE];

CIRC_BUFFER(spiTxBuffer, _txBuffer, SPI_BUFFER_SIZE);
CIRC_BUFFER(spiRxBuffer, _rxBuffer, SPI_BUFFER_SIZE);

volatile bool SPI_ACTIVE = FALSE;
volatile uint32_t reqNumBytes = 0U;

bool initSPI(uint8_t mode, uint8_t clock)
{

    // Set MOSI and SCK output
    SPI_DDR |= _BV(SPI_MOSI) | _BV(SPI_SCK);

    SPI_PORT |= _BV(SPI_MOSI) | _BV(SPI_SCK);

    // SS
    SPI_DDR |= _BV(SPI_SS);

    SPI_PORT |= _BV(SPI_SS);

    // Enable interrupt, enbale spi, set master, mode and clock divider
    SPCR = _BV(SPIE) | _BV(SPE) | _BV(MSTR) | (mode << CPHA) | (clock << SPR0);

    return TRUE;
}

bool triggerSendSPI(void)
{
    if (!SPI_ACTIVE)
    {
        SPI_ACTIVE = TRUE;

        if (!isEmptyBuffer(&spiTxBuffer))
        {
            uint8_t byte;

            if (getByteBuffer(&spiTxBuffer, &byte))
            {

                SPDR = byte;
                return TRUE;
            }
        }
    }

    return FALSE;
}

bool triggerReceiveSPI(uint32_t numBytes)
{
    reqNumBytes = numBytes;

    if (!SPI_ACTIVE)
    {
        SPI_ACTIVE = TRUE;

        SPDR = 0x00;
    }

    return TRUE;
}

bool sendByteSPI(uint8_t byte)
{
    if (writeByteBuffer(&spiTxBuffer, byte))
    {
        return TRUE;
    }

    return FALSE;
}

bool getByteSPI(uint8_t *byte)
{
    uint8_t tempByte = 0;
    if (getByteBuffer(&spiRxBuffer, &tempByte))
    {
        *byte = tempByte;
        return TRUE;
    }

    return FALSE;
}

ISR(SPI_STC_vect)
{

    // Receive data
    if (!isFullBuffer(&spiRxBuffer))
    {
        writeByteBuffer(&spiRxBuffer, SPDR);
    }

    // Send data
    if (!isEmptyBuffer(&spiTxBuffer))
    {
        uint8_t byte;

        getByteBuffer(&spiTxBuffer, &byte);
        SPDR = byte;
    }
    else if (reqNumBytes > 0U)
    {
        reqNumBytes--;
        SPDR = 0x00; // Fill with 0
    }
    else
    {
        // Done
        SPI_ACTIVE = FALSE;
    }
}

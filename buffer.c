#include <avr/io.h>
#include "util.h"
#include "buffer.h"

#define BUFF_SIZE 300

bool getByteBuffer(CircBuffer_t *buffer, uint8_t *byte)
{

    if (buffer->length > 0U)
    {
        *byte = buffer->_buffer[buffer->indexRead];
    }
    else
    {
        return FALSE;
    }

    buffer->length--;
    buffer->indexRead++;
    buffer->indexRead %= buffer->buff_size;

    return TRUE;
}

bool writeByteBuffer(CircBuffer_t *buffer, uint8_t byte)
{

    if (buffer->length < buffer->buff_size)
    {
        buffer->_buffer[buffer->indexWrite] = byte;
    }
    else
    {
        return FALSE;
    }

    buffer->indexWrite++;
    buffer->length++;
    buffer->indexWrite %= buffer->buff_size;

    return TRUE;
}

uint32_t getBufferFreeSpace(CircBuffer_t *buffer)
{
    return buffer->length;
}

bool isEmptyBuffer(CircBuffer_t *buffer) {
    return buffer->length == 0U;
}

bool isFullBuffer(CircBuffer_t *buffer) {
    return buffer->length == buffer->buff_size;
}
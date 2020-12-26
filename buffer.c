#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

#include <util.h>
#include <buffer.h>

bool getByteBuffer(CircBuffer_t *buffer, uint8_t *byte) {
    bool empty = FALSE;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        empty = isEmptyBuffer(buffer);

        if (!empty) {
            *byte = buffer->_buffer[buffer->indexRead];
        } else {
            return FALSE;
        }

        buffer->length--;
        buffer->indexRead++;
        buffer->indexRead %= buffer->buff_size;
    }

    return TRUE;
}

bool writeByteBuffer(CircBuffer_t *buffer, uint8_t byte) {
    bool full = FALSE;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        full = isFullBuffer(buffer);

        if (!full) {
            buffer->_buffer[buffer->indexWrite] = byte;
        } else {
            return FALSE;
        }

        buffer->length++;
        buffer->indexWrite++;
        buffer->indexWrite %= buffer->buff_size;
    }
    return TRUE;
}

uint32_t getBufferFreeSpace(CircBuffer_t *buffer) {
    return buffer->length;
}

bool isEmptyBuffer(CircBuffer_t *buffer) {
    return buffer->length == 0U;
}

bool isFullBuffer(CircBuffer_t *buffer) {
    return buffer->length == buffer->buff_size;
}
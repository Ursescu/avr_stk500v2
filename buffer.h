#ifndef _BUFFER_H_
#define _BUFFER_H_

#include <util.h>

typedef struct CircBuffer {
    uint32_t length;
    uint32_t indexRead;
    uint32_t buff_size;
    uint32_t indexWrite;
    uint8_t *_buffer;
} CircBuffer_t;

#define CIRC_BUFFER(name, buffer, len) \
    static CircBuffer_t name = {       \
        .length = 0U,                  \
        .indexRead = 0U,               \
        .indexWrite = 0U,              \
        .buff_size = (len),            \
        ._buffer = (uint8_t *)(buffer)}

bool getByteBuffer(CircBuffer_t *buffer, uint8_t *byte);
bool writeByteBuffer(CircBuffer_t *buffer, uint8_t byte);
uint32_t getBufferFreeSpace(CircBuffer_t *buffer);
bool isEmptyBuffer(CircBuffer_t *buffer);
bool isFullBuffer(CircBuffer_t *buffer);

#endif
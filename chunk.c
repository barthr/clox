#include <stdlib.h>

#include "chunk.h"
#include "memory.h"
#include "value.h"
#include <glib.h>

void initChunk(Chunk* chunk)
{
    *chunk = (Chunk) {
        .code = g_array_new(true, true, 1),
        .lines = g_array_new(true, true, 1),
    };
    initValueArray(&chunk->constants);
}

void freeChunk(Chunk* chunk)
{
    g_array_free(chunk->code, true);
    g_array_free(chunk->lines, true);
    freeValueArray(&chunk->constants);
    initChunk(chunk);
}

void writeChunk(Chunk* chunk, uint8_t byte, int line)
{
    g_array_append(chunk->code, byte);
    g_array_append(chunk->lines, line);
}

int addConstant(Chunk* chunk, Value value)
{
    writeValueArray(&chunk->constants, value);
    return length(&chunk->constants) - 1;
}
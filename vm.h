#ifndef clox_vm_h
#define clox_vm_h
#include "chunk.h"

typedef struct
{
    Chunk* chunk;
    uint8_t* ip; // instruction pointer
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

void initVM(VM* vm);
void freeVM(VM* vm);
InterpretResult interpret(VM* vm, Chunk* chunk);

#endif
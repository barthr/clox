#pragma once

#include "chunk.h"
#include "value.h"

#define STACK_MAX 256

typedef struct
{
    Chunk* chunk;
    uint8_t* ip;  // instruction pointer
    Value stack[STACK_MAX];
    Value* stackTop;
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

void initVM(VM* vm);
void freeVM(VM* vm);
InterpretResult interpret(VM* vm, const char* source);
void push(VM* vm, Value value);
Value pop(VM* vm);
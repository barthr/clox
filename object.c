#define _GNU_SOURCE
#include "object.h"
#include "memory.h"
#include "value.h"
#include "vm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Obj allocateObject(ObjType type)
{
    return (Obj) {
        .type = type
    };
}

static ObjString* allocateString(char* chars, int length)
{
    ObjString* obj = malloc(sizeof(ObjString));
    *obj = (ObjString) {
        .chars = chars,
        .length = length,
        .obj = allocateObject(OBJ_STRING)
    };
    return obj;
}

ObjString* copyString(const char* chars, int length)
{
    return allocateString(strndup(chars, length), length);
}

ObjString* takeString(const char* chars, int length)
{
    return allocateString(chars, length);
}

void printObject(Value value)
{
    switch (OBJ_TYPE(value)) {
    case OBJ_STRING:
        printf("%s", AS_CSTRING(value));
        break;
    default:
        break;
    }
}
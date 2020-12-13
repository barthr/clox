#define _GNU_SOURCE
#include "object.h"
#include "memory.h"
#include "value.h"
#include "vm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALLOCATE_OBJ(vm, type, objectType) \
    (type*)allocateObject(vm, sizeof(type), objectType)

static Obj* allocateObject(VM* vm, size_t size, ObjType type)
{
    Obj* object = (Obj*)reallocate(NULL, 0, size);
    object->type = type;
    object->next = vm->objects;
    vm->objects = object;
    return object;
}

static ObjString* allocateString(VM* vm, char* chars, int length)
{
    ObjString* obj = ALLOCATE_OBJ(vm, ObjString, OBJ_STRING);
    obj->chars = chars;
    obj->length = length;
    return obj;
}

ObjString* copyString(VM* vm, const char* chars, int length)
{
    return allocateString(vm, strndup(chars, length), length);
}

ObjString* takeString(VM* vm, char* chars, int length)
{
    return allocateString(vm, chars, length);
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
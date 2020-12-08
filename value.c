#include <stdio.h>

#include "memory.h"
#include "value.h"
#include <glib.h>

void initValueArray(ValueArray* array)
{
    *array = (ValueArray) {
        .values = g_array_new(true, true, 0),
    };
}

void writeValueArray(ValueArray* array, Value value)
{
    g_array_append_val(array->values, value);
}

void freeValueArray(ValueArray* array)
{
    g_array_free(array->values, true);
    initValueArray(array);
}

void printValue(Value value)
{
    printf("%g", AS_NUMBER(value));
}

int length(ValueArray* array)
{
    return g_array_get_element_size(array->values);
}
#pragma once
#include "object.h"
#include "scanner.h"
#include "vm.h"
#include <stdbool.h>

bool compile(VM* vm, const char* source, Chunk* chunk);

#include <stdbool.h>
#include <stdio.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "vm.h"

bool compile(const char* source, Chunk* chunk)
{
    Scanner scanner;
    initScanner(&scanner, source);

    advance(&scanner)
        consume()
}

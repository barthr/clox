#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "vm.h"

typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

static void errorAt(Parser* parser, Token* token, const char* message)
{
    if (parser->panicMode) {
        return;
    }
    parser->panicMode = true;
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, "at end");
    } else if (token->type == TOKEN_ERROR) {
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser->hadError = true;
}

static void error(Parser* parser, const char* message)
{
    errorAt(parser, &parser->previous, message);
}

static void errorAtCurrent(Parser* parser, const char* message)
{
    errorAt(parser, &parser->current, message);
}

static void advance(Parser* parser, Scanner* scanner)
{
    parser->previous = parser->current;

    for (;;) {
        parser->current = scanToken(scanner);
        if (parser->current.type != TOKEN_ERROR) {
            break;
        }
        errorAtCurrent(parser, parser->current.start);
    }
}

static void consume(Parser* parser, Scanner* scanner, TokenType type, const char* message)
{
    if (parser->current.type == type) {
        advance(parser, scanner);
        return;
    }

    errorAtCurrent(parser, message);
}

// static void emitByte(Parser* parser, uint8_t byte)
// {
//     writeChunk(currentChunk(), byte, parser->previous.line);
// }

bool compile(const char* source, Chunk* chunk)
{
    Scanner scanner;
    initScanner(&scanner, source);

    Parser parser = (Parser) {
        .hadError = false,
        .panicMode = false,
    };

    advance(&scanner, &parser);
    expression(&scanner);
    consume(&parser, &scanner, TOKEN_EOF, "Expect end of expression.");

    return !parser.hadError;
}

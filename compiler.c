#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "vm.h"

typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
    Chunk* currentChunk
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

static void emitByte(Parser* parser, uint8_t byte)
{
    writeChunk(parser->currentChunk, byte, parser->previous.line);
}

static void emitReturn(Parser* parser)
{
    emitByte(parser, OP_RETURN);
}

static uint8_t makeConstant(Parser* parser, Value value)
{
    int constant = addConstant(parser->currentChunk, value);
    if (constant > UINT8_MAX) {
        error(parser, "Too many constants in one chunk.");
        return 0;
    }
    return constant;
}

static void emitConstant(Parser* parser, Value value)
{
    emitBytes(parser, OP_CONSTANT, makeConstant(parser, value));
}

static void endCompiler(Parser* parser)
{
    emitReturn(parser);
}

static void grouping(Parser* parser, Scanner* scanner)
{
    expression(parser);
    consume(parser, scanner, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(Parser* parser)
{
    double value = strod(parser->previous.start, NULL);
    emitConstant(parser, value);
}

static void unary(Parser* parser)
{
    TokenType operatorType = parser->previous.type;
    expression(parser);

    switch (operatorType) {
    case TOKEN_MINUS:
        emitByte(parser, OP_NEGATE);
        break;
    default:
        return;
    }
}

static void expression(Parser* parser)
{
}

static void emitBytes(Parser* parser, uint8_t byte1, uint8_t byte2)
{
    emitByte(parser, byte1);
    emitByte(parser, byte2);
}

bool compile(const char* source, Chunk* chunk)
{
    Scanner scanner;
    initScanner(&scanner, source);

    Parser parser = (Parser) {
        .hadError = false,
        .panicMode = false,
        .currentChunk = chunk,
    };

    advance(&parser, &scanner);
    expression(&scanner);
    consume(&parser, &scanner, TOKEN_EOF, "Expect end of expression.");
    endCompiler(&parser);

    return !parser.hadError;
}

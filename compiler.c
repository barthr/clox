#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "vm.h"
#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
    Chunk* currentChunk;
} Parser;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT, // =
    PREC_OR, // or
    PREC_AND, // and
    PREC_EQUALITY, // == !=
    PREC_COMPARISON, // < > <= >=
    PREC_TERM, // + -
    PREC_FACTOR, // * /
    PREC_UNARY, // ! -
    PREC_CALL, // . ()
    PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(Parser(*), Scanner(*));

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

static void expression();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence, Parser* parser, Scanner* scanner);

static void
errorAt(Parser* parser, Token* token, const char* message)
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

static void emitBytes(Parser* parser, uint8_t byte1, uint8_t byte2)
{
    emitByte(parser, byte1);
    emitByte(parser, byte2);
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
#ifdef DEBUG_PRINT_CODE
    if (!parser->hadError) {
        disassembleChunk(parser->currentChunk, "code");
    }
#endif
}

static void binary(Parser* parser, Scanner* scanner)
{
    TokenType operatorType = parser->previous.type;

    ParseRule* rule = getRule(operatorType);
    parsePrecedence(rule->precedence + 1, parser, scanner);

    switch (operatorType) {
    case TOKEN_PLUS:
        emitByte(parser, OP_ADD);
        break;
    case TOKEN_MINUS:
        emitByte(parser, OP_SUBTRACT);
        break;
    case TOKEN_STAR:
        emitByte(parser, OP_MULTIPLY);
        break;
    case TOKEN_SLASH:
        emitByte(parser, OP_DIVIDE);
        break;
    default:
        return; // Unreachable.
    }
}

static void literal(Parser* parser, Scanner* scanner)
{
    switch (parser->previous.type) {
    case TOKEN_FALSE:
        emitByte(parser, OP_FALSE);
        break;
    case TOKEN_TRUE:
        emitByte(parser, OP_TRUE);
        break;
    case TOKEN_NIL:
        emitByte(parser, OP_NIL);
        break;
    default:
        return;
    }
}

static void grouping(Parser* parser, Scanner* scanner)
{
    expression(parser, scanner);
    consume(parser, scanner, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(Parser* parser, Scanner* scanner)
{
    double value = strtod(parser->previous.start, NULL);
    emitConstant(parser, NUMBER_VAL(value));
}

static void unary(Parser* parser, Scanner* scanner)
{
    TokenType operatorType = parser->previous.type;

    parsePrecedence(PREC_UNARY, parser, scanner);

    switch (operatorType) {
    case TOKEN_MINUS:
        emitByte(parser, OP_NEGATE);
        break;
    default:
        return;
    }
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = { grouping, NULL, PREC_NONE },
    [TOKEN_RIGHT_PAREN] = { NULL, NULL, PREC_NONE },
    [TOKEN_LEFT_BRACE] = { NULL, NULL, PREC_NONE },
    [TOKEN_RIGHT_BRACE] = { NULL, NULL, PREC_NONE },
    [TOKEN_COMMA] = { NULL, NULL, PREC_NONE },
    [TOKEN_DOT] = { NULL, NULL, PREC_NONE },
    [TOKEN_MINUS] = { unary, binary, PREC_TERM },
    [TOKEN_PLUS] = { NULL, binary, PREC_TERM },
    [TOKEN_SEMICOLON] = { NULL, NULL, PREC_NONE },
    [TOKEN_SLASH] = { NULL, binary, PREC_FACTOR },
    [TOKEN_STAR] = { NULL, binary, PREC_FACTOR },
    [TOKEN_BANG] = { NULL, NULL, PREC_NONE },
    [TOKEN_BANG_EQUAL] = { NULL, NULL, PREC_NONE },
    [TOKEN_EQUAL] = { NULL, NULL, PREC_NONE },
    [TOKEN_EQUAL_EQUAL] = { NULL, NULL, PREC_NONE },
    [TOKEN_GREATER] = { NULL, NULL, PREC_NONE },
    [TOKEN_GREATER_EQUAL] = { NULL, NULL, PREC_NONE },
    [TOKEN_LESS] = { NULL, NULL, PREC_NONE },
    [TOKEN_LESS_EQUAL] = { NULL, NULL, PREC_NONE },
    [TOKEN_IDENTIFIER] = { NULL, NULL, PREC_NONE },
    [TOKEN_STRING] = { NULL, NULL, PREC_NONE },
    [TOKEN_NUMBER] = { number, NULL, PREC_NONE },
    [TOKEN_AND] = { NULL, NULL, PREC_NONE },
    [TOKEN_CLASS] = { NULL, NULL, PREC_NONE },
    [TOKEN_ELSE] = { NULL, NULL, PREC_NONE },
    [TOKEN_FALSE] = { literal, NULL, PREC_NONE },
    [TOKEN_FOR] = { NULL, NULL, PREC_NONE },
    [TOKEN_FUN] = { NULL, NULL, PREC_NONE },
    [TOKEN_IF] = { NULL, NULL, PREC_NONE },
    [TOKEN_NIL] = { literal, NULL, PREC_NONE },
    [TOKEN_OR] = { NULL, NULL, PREC_NONE },
    [TOKEN_PRINT] = { NULL, NULL, PREC_NONE },
    [TOKEN_RETURN] = { NULL, NULL, PREC_NONE },
    [TOKEN_SUPER] = { NULL, NULL, PREC_NONE },
    [TOKEN_THIS] = { NULL, NULL, PREC_NONE },
    [TOKEN_TRUE] = { literal, NULL, PREC_NONE },
    [TOKEN_VAR] = { NULL, NULL, PREC_NONE },
    [TOKEN_WHILE] = { NULL, NULL, PREC_NONE },
    [TOKEN_ERROR] = { NULL, NULL, PREC_NONE },
    [TOKEN_EOF] = { NULL, NULL, PREC_NONE },
};

static void parsePrecedence(Precedence precedence, Parser* parser, Scanner* scanner)
{
    advance(parser, scanner);

    ParseFn prefixRule = getRule(parser->previous.type)->prefix;
    if (prefixRule == NULL) {
        error(parser, "Expect expression.");
        return;
    }

    prefixRule(parser, scanner);
    while (precedence <= getRule(parser->current.type)->precedence) {
        advance(parser, scanner);
        ParseFn infixRule = getRule(parser->previous.type)->infix;
        infixRule(parser, scanner);
    }
}

static ParseRule* getRule(TokenType type)
{
    return &rules[type];
}

static void expression(Parser* parser, Scanner* scanner)
{
    parsePrecedence(PREC_ASSIGNMENT, parser, scanner);
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
    expression(&parser, &scanner);
    consume(&parser, &scanner, TOKEN_EOF, "Expect end of expression.");
    endCompiler(&parser);

    return !parser.hadError;
}

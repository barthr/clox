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

typedef struct {
    Scanner* scanner;
    Parser* parser;
    VM* vm;
} Compiler;

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

typedef void (*ParseFn)(Compiler* compiler);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

static void expression();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence, Compiler* compiler);

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

static void advance(Compiler* compiler)
{
    Parser* parser = compiler->parser;
    Scanner* scanner = compiler->scanner;

    parser->previous = parser->current;
    for (;;) {
        parser->current = scanToken(scanner);
        if (parser->current.type != TOKEN_ERROR) {
            break;
        }
        errorAtCurrent(parser, parser->current.start);
    }
}

static void consume(Compiler* compiler, TokenType type, const char* message)
{
    Parser* parser = compiler->parser;
    if (parser->current.type == type) {
        advance(compiler);
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

static void endCompiler(Compiler* compiler)
{
    Parser* parser = compiler->parser;
    emitReturn(parser);
#ifdef DEBUG_PRINT_CODE
    if (!parser->hadError) {
        disassembleChunk(parser->currentChunk, "code");
    }
#endif
}

static void binary(Compiler* compiler)
{
    Parser* parser = compiler->parser;
    TokenType operatorType = compiler->parser->previous.type;

    ParseRule* rule = getRule(operatorType);
    parsePrecedence(rule->precedence + 1, compiler);

    switch (operatorType) {
    case TOKEN_BANG_EQUAL:
        emitBytes(parser, OP_EQUAL, OP_NOT);
        break;
    case TOKEN_EQUAL_EQUAL:
        emitByte(parser, OP_EQUAL);
        break;
    case TOKEN_GREATER:
        emitByte(parser, OP_GREATER);
        break;
    case TOKEN_GREATER_EQUAL:
        emitBytes(parser, OP_LESS, OP_NOT);
        break;
    case TOKEN_LESS:
        emitByte(parser, OP_LESS);
        break;
    case TOKEN_LESS_EQUAL:
        emitBytes(parser, OP_GREATER, OP_NOT);
        break;
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

static void literal(Compiler* compiler)
{
    Parser* parser = compiler->parser;
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

static void grouping(Compiler* compiler)
{
    expression(compiler);
    consume(compiler, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(Compiler* compiler)
{
    Parser* parser = compiler->parser;
    double value = strtod(parser->previous.start, NULL);
    emitConstant(parser, NUMBER_VAL(value));
}

static void string(Compiler* compiler)
{
    VM* vm = compiler->vm;
    Parser* parser = compiler->parser;
    emitConstant(parser, OBJ_VAL(copyString(vm, parser->previous.start + 1, parser->previous.length - 2)));
}

static void unary(Compiler* compiler)
{
    Parser* parser = compiler->parser;
    TokenType operatorType = parser->previous.type;

    parsePrecedence(PREC_UNARY, compiler);

    switch (operatorType) {
    case TOKEN_BANG:
        emitByte(parser, OP_NOT);
        break;
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
    [TOKEN_BANG] = { unary, NULL, PREC_NONE },
    [TOKEN_BANG_EQUAL] = { NULL, binary, PREC_EQUALITY },
    [TOKEN_EQUAL] = { NULL, NULL, PREC_NONE },
    [TOKEN_EQUAL_EQUAL] = { NULL, binary, PREC_COMPARISON },
    [TOKEN_GREATER] = { NULL, binary, PREC_COMPARISON },
    [TOKEN_GREATER_EQUAL] = { NULL, binary, PREC_COMPARISON },
    [TOKEN_LESS] = { NULL, binary, PREC_COMPARISON },
    [TOKEN_LESS_EQUAL] = { NULL, binary, PREC_COMPARISON },
    [TOKEN_IDENTIFIER] = { NULL, NULL, PREC_NONE },
    [TOKEN_STRING] = { string, NULL, PREC_NONE },
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

static void parsePrecedence(Precedence precedence, Compiler* compiler)
{
    advance(compiler);

    Parser* parser = compiler->parser;

    ParseFn prefixRule = getRule(parser->previous.type)->prefix;
    if (prefixRule == NULL) {
        error(parser, "Expect expression.");
        return;
    }

    prefixRule(compiler);
    while (precedence <= getRule(parser->current.type)->precedence) {
        advance(compiler);
        ParseFn infixRule = getRule(parser->previous.type)->infix;
        infixRule(compiler);
    }
}

static ParseRule* getRule(TokenType type)
{
    return &rules[type];
}

static void expression(Compiler* compiler)
{
    parsePrecedence(PREC_ASSIGNMENT, compiler);
}

bool compile(VM* vm, const char* source, Chunk* chunk)
{
    Scanner scanner;
    initScanner(&scanner, source);

    Parser parser = (Parser) {
        .hadError = false,
        .panicMode = false,
        .currentChunk = chunk,
    };

    Compiler compiler = (Compiler) {
        .parser = &parser,
        .scanner = &scanner,
        .vm = vm
    };

    advance(&compiler);
    expression(&compiler);
    consume(&compiler, TOKEN_EOF, "Expect end of expression.");
    endCompiler(&compiler);

    return !parser.hadError;
}

#include "scanner.h"
#include <stdbool.h>
#include <string.h>

void initScanner(Scanner* scanner, const char* source)
{
    *scanner = (Scanner) {
        .start = 0,
        .current = source,
        .line = 1,
    };
}

static char advance(Scanner* scanner)
{
    scanner->current++;
    return scanner->current[-1];
}

static char peek(Scanner* scanner)
{
    return *scanner->current;
}

static char peekNext(Scanner* scanner)
{
    if (isAtEnd(scanner)) {
        return '\0';
    }
    return scanner->current[1];
}

static bool match(Scanner* scanner, char expected)
{
    if (isAtEnd(scanner)) {
        return false;
    }
    if (*scanner->current != expected) {
        return false;
    }
    scanner->current++;
    return true;
}

static bool isAtEnd(Scanner* scanner)
{
    return *scanner->current == '\0';
}

static Token makeToken(Scanner* scanner, TokenType type)
{
    return (Token) {
        .type = type,
        .start = scanner->start,
        .length = scanner->current - scanner->start,
        .line = scanner->line,
    };
}

static Token errorToken(Scanner* scanner, const char* message)
{
    return (Token) {
        .type = TOKEN_ERROR,
        .start = message, // point to error message
        .length = strlen(message),
        .line = scanner->line,
    };
}

void skipWhiteSpace(Scanner* scanner)
{
    for (;;) {
        char c = peek(scanner);
        switch (c) {
        case ' ':
        case '\r':
        case '\t':
            advance(scanner);
            break;
        case '\n':
            scanner->line++;
            advance(scanner);
            break;
        case '/':
            if (peekNext(scanner) == '/') {
                while (peek(scanner) != '\n' && !isAtEnd(scanner)) {
                    advance(scanner);
                }
            }
            return;
        default:
            return;
        }
    }
}

Token scanToken(Scanner* scanner)
{
    skipWhiteSpace(scanner);
    scanner->start = scanner->current;

    if (isAtEnd(scanner)) {
        return makeToken(scanner, TOKEN_EOF);
    }

    char c = advance(scanner);
    switch (c) {
    case '(':
        return makeToken(scanner, TOKEN_LEFT_PAREN);
    case ')':
        return makeToken(scanner, TOKEN_RIGHT_PAREN);
    case '{':
        return makeToken(scanner, TOKEN_LEFT_BRACE);
    case '}':
        return makeToken(scanner, TOKEN_RIGHT_BRACE);
    case ';':
        return makeToken(scanner, TOKEN_SEMICOLON);
    case ',':
        return makeToken(scanner, TOKEN_COMMA);
    case '.':
        return makeToken(scanner, TOKEN_DOT);
    case '-':
        return makeToken(scanner, TOKEN_MINUS);
    case '+':
        return makeToken(scanner, TOKEN_PLUS);
    case '/':
        return makeToken(scanner, TOKEN_SLASH);
    case '*':
        return makeToken(scanner, TOKEN_STAR);
    case '!':
        return makeToken(scanner, match(scanner, '=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
    case '=':
        return makeToken(scanner, match(scanner, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
    case '<':
        return makeToken(scanner, match(scanner, '=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
    case '>':
        return makeToken(scanner, match(scanner, '=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
    }
    return errorToken(scanner, "Unexpected character.");
}

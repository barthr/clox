#include "scanner.h"
#include <ctype.h>
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

static bool isAtEnd(Scanner* scanner)
{
    return *scanner->current == '\0';
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

static TokenType checkKeyword(Scanner* scanner, int start, int length, char* rest, TokenType type)
{
    if (scanner->current - scanner->start == start + length && memcmp(scanner->start + 1, rest, length)) {
        return type;
    }
    return TOKEN_IDENTIFIER;
}

static TokenType identifierType(Scanner* scanner)
{
    switch (scanner->start[0]) {
    case 'a':
        return checkKeyword(scanner, 1, 2, "nd", TOKEN_AND);
    case 'c':
        return checkKeyword(scanner, 1, 4, "lass", TOKEN_CLASS);
    case 'e':
        return checkKeyword(scanner, 1, 3, "lse", TOKEN_ELSE);
    case 'i':
        return checkKeyword(scanner, 1, 1, "f", TOKEN_IF);
    case 'n':
        return checkKeyword(scanner, 1, 2, "il", TOKEN_NIL);
    case 'o':
        return checkKeyword(scanner, 1, 1, "r", TOKEN_OR);
    case 'p':
        return checkKeyword(scanner, 1, 4, "rint", TOKEN_PRINT);
    case 'r':
        return checkKeyword(scanner, 1, 5, "eturn", TOKEN_RETURN);
    case 's':
        return checkKeyword(scanner, 1, 4, "uper", TOKEN_SUPER);
    case 'v':
        return checkKeyword(scanner, 1, 2, "ar", TOKEN_VAR);
    case 'w':
        return checkKeyword(scanner, 1, 4, "hile", TOKEN_WHILE);
    }
    return TOKEN_IDENTIFIER;
}

Token identifier(Scanner* scanner)
{
    char current = peek(scanner);
    while (isalpha(current) || isdigit(current)) {
        advance(scanner);
    }

    return makeToken(scanner, identifierType(scanner));
}

Token number(Scanner* scanner)
{
    while (isdigit(peek(scanner))) {
        advance(scanner);
    }

    if (peek(scanner) == '.' && isdigit(peekNext(scanner))) {
        advance(scanner); // consume '.'
        while (isdigit(peek(scanner))) {
            advance(scanner);
        }
    }
    return makeToken(scanner, TOKEN_NUMBER);
}

Token string(Scanner* scanner)
{
    while (peek(scanner) != '"' && !isAtEnd(scanner)) {
        if (peek(scanner) == '\n') {
            scanner->line++;
        }
        advance(scanner);
    }

    if (isAtEnd(scanner)) {
        return errorToken(scanner, "Unterminated string.");
    }
    advance(scanner);
    return makeToken(scanner, TOKEN_STRING);
}

Token scanToken(Scanner* scanner)
{
    skipWhiteSpace(scanner);
    scanner->start = scanner->current;

    if (isAtEnd(scanner)) {
        return makeToken(scanner, TOKEN_EOF);
    }

    char c = advance(scanner);
    if (isalpha(c)) {
        return identifier(scanner);
    }
    if (isdigit(c)) {
        return number(scanner);
    }

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
    case '"':
        return string(scanner);
    }
    return errorToken(scanner, "Unexpected character.");
}

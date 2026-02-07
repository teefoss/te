//
//  parser.c
//
//  Created by Thomas Foster on 6/12/25.
//

#include "editor.h"

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOK_STR_LEN 64

typedef enum {
    TOK_EOF,
    TOK_STRING,
    TOK_NUMBER,
    TOK_IDENTIFIER,
    TOK_SYMBOL, // single non-alphanumeric char
    TOK_UNKNOWN
} TokenType;

typedef struct {
    TokenType type;
    char string[TOK_STR_LEN];
    int number;
    char symbol;
} Token;

static char *   input;
static char *   _c;             // pointer to parse location in `input`
static int      line_num = 1;   // current line number during parsing
static Token    peek;           // the next token
static Token    token;          // the current token

static void ParseError(const char * fmt, ...)
{
    fprintf(stderr, "Parse error on line %d: ", line_num);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
}

static int GetChar(void)
{
    return *_c++;
}

static void UngetChar(void)
{
    --_c;
}

static void GetToken(void)
{
    int c;

    while ( isspace(c = GetChar()) ) {
        if ( c == '\n' ) {
            line_num++;
        }
    }

    if ( c == EOF || c == '\0' ) {
        peek.type = (TokenType)EOF;
        return;
    }

    if ( isalpha(c) || c == '_' ) {
        // Read identifier into peek.string
        char * p = peek.string;
        do {
            *p++ = (char)c;
            if ( p == &peek.string[TOK_STR_LEN] ) break;
        } while ( (c = GetChar()) != EOF && (isalnum(c) || c == '_') );

        UngetChar();
        *p = '\0';
        peek.type = TOK_IDENTIFIER;
    } else if ( isdigit(c) ) {
        // Get number
        UngetChar();
        long n = strtol(_c, &_c, 0);
        if ( n <= INT_MAX ) {
            peek.number = (int)n;
        } else {
            ParseError("integer too large");
        }
        peek.type = TOK_NUMBER;
    } else if ( c == '"' ) {
        char * s = peek.string;
        char last_char = (char)c;
        c = GetChar(); // Advance past the "

        // Fill token string until a " is found, etc
        while ( 1 ) {
            if ( c == EOF ) break;
            if ( c == '"' && last_char != '\\' ) break; // End of string
            if ( s == peek.string + TOK_STR_LEN - 1 ) break; // No room.

            *s++ = (char)c;
            last_char = (char)c;
            c = GetChar();
        }

        *s = '\0';
        peek.type = TOK_STRING;
    } else if ( ispunct(c) ) {
        peek.symbol = (char)c;
        peek.type = TOK_SYMBOL;
    }
}

int GetTokenNumber(void) {
    return peek.number;
}

char * GetTokenString(void) {
    return peek.string;
}

bool Accept(TokenType token_type)
{
    if ( token_type == peek.type ) {
        token = peek;
        GetToken();
        return true;
    }

    return false;
}

void Expect(TokenType token_type)
{
    if ( !Accept(token_type) ) {
        // TODO: token string
        ParseError("expected token of type %d\n", token_type);
    }
}

bool AcceptInt(int * out)
{
    if ( Accept(TOK_NUMBER) ) {
        *out = token.number;
        return true;
    }

    return false;
}

int ExpectInt(void)
{
    int n;
    if ( AcceptInt(&n) ) {
        return n;
    }

    ParseError("expected integer");
    return 0;
}

bool AcceptIdent(char * out, size_t len)
{
    if ( Accept(TOK_IDENTIFIER) ) {
        strncpy(out, token.string, len);
        out[len - 1] = '\0';
        return true;
    }

    return false;
}

void MatchIdent(const char * ident)
{
    char str[256];
    if ( !AcceptIdent(str, sizeof(str)) || strcmp(ident, str) != 0 ) {
        ParseError("expected identifier '%s'", ident);
    }
}

void ExpectIdent(char * out, size_t len)
{
    if ( !AcceptIdent(out, len) ) {
        ParseError("expected identifier");
    }
}

void MatchInt(int n)
{
    int check;
    if ( !AcceptInt(&check) && check == n ) {
        ParseError("expected integer '%d'", n);
    }
}

bool AcceptString(char * out, size_t len)
{
    if ( Accept(TOK_STRING) ) {
        strncpy(out, token.string, len);
        out[len - 1] = '\0';
        return true;
    }

    return false;
}

bool AcceptSymbol(char * out)
{
    if ( Accept(TOK_SYMBOL) ) {
        *out = peek.symbol;
        return true;
    }

    return false;
}

void MatchSymbol(char s)
{
    char check;
    if ( !AcceptSymbol(&check) || check != s ) {
        ParseError("expected symbol '%c'", s);
    }
}

char ExpectSymbol(void)
{
    char s;
    if ( AcceptSymbol(&s) ) {
        return s;
    }

    ParseError("expected symbol");
    return 0;
}

void ExpectString(char * out, size_t len)
{
    if ( !AcceptString(out, len) ) {
        ParseError("expected string");
    }
}

static size_t _FileSize(FILE * file)
{
    long current = ftell(file);
    fseek(file, 0, SEEK_END);
    long end = ftell(file);
    fseek(file, current, SEEK_SET);

    return (size_t)end;
}

// TODO: this should be the version for tflib
static void
StripComments(char * buf, char ch)
{
    bool in_string = false; // Currently between two "

    int i, j;
    for ( i = 0, j = 0; buf[i] != '\0'; i++ ) {
        if ( buf[i] == '"' ) {
            in_string = !in_string;
        }

        if ( buf[i] == ch && !in_string ) {
            while ( buf[i] != '\n' && buf[i] != '\0' ) {
                i++;
            }
        }

        buf[j++] = buf[i];
    }

    buf[j] = '\0';
}

bool BeginParsing(const char * path)
{
    line_num = 1;

    FILE * file = fopen(path, "r");
    if ( file == NULL ) {
        return false;
    }

    size_t size = _FileSize(file);
    input = malloc(size + 1);

    if ( input == NULL ) {
        fprintf(stderr, "parse error: could not allocate input buffer\n");
        exit(1);
    }

    fread(input, 1, size, file);
    input[size] = '\0';
    fclose(file);

    StripComments(input, '#');
    _c = input;
    GetToken();

    return true;
}

void EndParsing(void)
{
    free(input);
}

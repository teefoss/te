//
//  parser.h
//  te
//
//  Created by Thomas Foster on 11/28/25.
//

#ifndef parser_h
#define parser_h

bool BeginParsing(const char * path);
void EndParsing(void);

// Optionally accept a token.
bool AcceptIdent(char * out, size_t len);
bool AcceptInt(int * out);
bool AcceptString(char * out, size_t len);
bool AcceptSymbol(char * out);

// Require a token of a specific type, but any value.
int  ExpectInt(void);
void ExpectString(char * out, size_t len);
void ExpectIdent(char * out, size_t len);

// Require a token of a specific type and value.
void MatchIdent(const char * ident);
void MatchInt(int n);
void MatchSymbol(char s);

#endif /* parser_h */

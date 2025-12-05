//
//  args.h
//  Command line argument helper functions.
//
//  Created by Thomas Foster on 11/3/24.
//

#ifndef args_h
#define args_h

#include <stdbool.h>

void LoadArgs(int argc, char ** argv);
bool ArgIsPresent(const char * arg);
const char * GetArg(int index);
int GetArgNum(const char * arg);

/**
 *  Get the string argument after `option`, checking optional alternative `alt`
 *  as well.
 *  - returns: Returns `NULL` if neither `option` nor `alt` were present. */
char * GetStrOption(const char * option, const char * alt);

bool GetIntOption(int arg, int * out);

/**
 *  Get the int argument after `option`, checking optional alternative `alt`
 *  as well.
 *  - returns: Returns false if neither `option` nor `alt` were present. */
bool GetIntOptionArg(const char * arg, const char * alt, int * out);

/**
 *  Get the number of option arguments after `arg` or `alt`. (An option argument
 *  is any that does not begin with a '-'.)
 */
int GetOptionCount(const char * arg, const char * alt, int * first_index);

#endif /* args_h */

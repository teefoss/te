//
//  args.c
//  zi23
//
//  Created by Thomas Foster on 11/3/24.
//

#include "args.h"
#include <string.h>
#include <stdlib.h>

static int     _argc;
static char ** _argv;

void LoadArgs(int argc, char ** argv)
{
    _argc = argc;
    _argv = argv;
}

int GetArgNum(const char * arg)
{
    for ( int i = 0; i < _argc; i++ ) {
        if ( strcmp(_argv[i], arg) == 0 ) {
            return i;
        }
    }

    return -1;
}

bool ArgIsPresent(const char * arg)
{
    return GetArgNum(arg) != -1;
}

const char * GetArg(int index)
{
    if ( index < 0 || index >= _argc ) {
        return NULL;
    }

    return _argv[index];
}

// Index is not -1 and is not the last argument. (There's at least one arg,
// i.e. the option, after it.)
static bool IsValidOptionIndex(int i)
{
    return i >= 0 && i + 1 < _argc;
}

char * GetStrOption(const char * arg, const char * alt)
{
    int i = GetArgNum(arg);

    if ( IsValidOptionIndex(i) ) {
        return _argv[i + 1];
    }

    if ( alt ) {
        i = GetArgNum(alt);

        if ( IsValidOptionIndex(i) ) {
            return _argv[i + 1];
        }
    }

    return NULL;
}

int GetOptionCount(const char * arg, const char * alt, int * first_index)
{
    int i = GetArgNum(arg);

    if ( !IsValidOptionIndex(i) ) {
        i = GetArgNum(alt);
        if ( !IsValidOptionIndex(i) ) {
            return 0;
        }
    }

    i++;
    *first_index = i;
    int num_options = 0;
    while ( i < _argc && _argv[i][0] != '-' ) {
        num_options++;
        i++;
    }

    return num_options;
}

bool GetIntOption(int arg, int * out)
{
    if ( arg < _argc ) {
        char * end;
        long i = strtol(_argv[arg], &end, 0);
        if ( *end == '\0' ) {
            *out = (int)i;
            return true;
        }
    }

    return false;
}

bool GetIntOptionArg(const char * arg, const char * alt, int * out)
{
    int i = GetArgNum(arg);
    if ( i == -1 ) {
        i = GetArgNum(alt);
        if ( i == -1 ) return false;
    }

    return GetIntOption(i, out);
}

//
//  config.c
//
//  Created by Thomas Foster on 7/17/25.
//

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#define MAX_KEY_LEN    64 // Update scanf formats if changed!
#define MAX_VAL_LEN     256 // Update scanf formats if changed!
#define COMMENT_CHAR    ';'

static bool
CaseCompare(const char * a, const char * b, size_t n)
{
    if ( a == NULL || b == NULL ) {
        return false;
    }
    
    for ( size_t i = 0; i < n; i++ ) {
        if ( toupper(a[i]) != toupper(b[i]) ) {
            return false;
        } else if ( a[i] == '\0' ) {
            return true;
        }
    }

    return true;
}

static const char * bool_str[] = { "no", "yes" };

static const Option *
FindOption(const Option * options, const char * name)
{
    if ( name == NULL ) {
        return NULL;
    }

    for ( const Option * opt = options; opt->type != CONFIG_NULL; opt++ ) {
        if ( CaseCompare(opt->name, name, MAX_KEY_LEN) ) {
            return opt;
        }
    }

    fprintf(stderr, "config file has unknown option '%s'\n", name);
    return NULL;
}

bool
SaveConfig(const Option * options, const char * path)
{
    FILE * file = fopen(path, "w");
    if ( file == NULL ) {
        fprintf(stderr, "%s error: failed to create '%s\n'", __func__, path);
        return false;
    }

    const Option * opt;
    for ( opt = options; opt->type != CONFIG_NULL; opt++ ) {

        // Print option name for those that have one.
        if ( opt->type < OPT_MISC ) {
            int count = fprintf(file, "%s ", opt->name);

            while ( count++ < 20 ) {
                fprintf(file, " ");
            }
        }

        switch ( opt->type ) {
            case CONFIG_BOOL:
                fprintf(file, "%s", bool_str[*(bool *)opt->value]);
                break;
            case CONFIG_OCT_INT:
                fprintf(file, "0%o", *(int *)opt->value);
                break;
            case CONFIG_DEC_INT:
                fprintf(file, "%d", *(int *)opt->value);
                break;
            case CONFIG_HEX_INT:
                fprintf(file, "0x%X", *(int *)opt->value);
                break;
            case CONFIG_FLOAT:
                fprintf(file, "%f", *(float *)opt->value);
                break;
            case CONFIG_DOUBLE:
                fprintf(file, "%lf", *(double *)opt->value);
                break;
            case CONFIG_COMMENT:
                fprintf(file, "%c %s", COMMENT_CHAR, (char *)opt->name);
                break;
            case CONFIG_STR:
                fprintf(file, "\"%s\"", (char *)opt->value);
                break;
            case CONFIG_BLANK_LINE:
                break;
            case CONFIG_NULL:
                break;
        }

        fprintf(file, "\n");
    }

    fclose(file);
    return true;
}

bool
LoadConfig(const Option * options, const char * path)
{
    FILE * file = fopen(path, "r");
    if ( file == NULL ) {
        return false; // No problem.
    }

    char line[512] = { 0 };

    while ( fgets(line, sizeof(line), file) != NULL ) {

        char key[MAX_KEY_LEN] = { 0 };
        char val[MAX_VAL_LEN] = { 0 };

        char * l = line;

        // Strip leading whitespace
        while ( isspace(*l) ) {
            l++;
        }

        // Strip comments
        char * comment_start = strchr(l, ';');
        if ( comment_start ) {
            *comment_start = '\0';
        }

        if ( sscanf(line, "%63s %[^\n]\n", key, val) != 2 ) {
            continue;
        }

        const Option * opt = FindOption(options, key);
        if ( opt == NULL ) {
            continue;
        }

        switch ( opt->type ) {
            case CONFIG_BOOL:
                if ( CaseCompare(val, "yes", MAX_VAL_LEN) ) {
                    *(bool *)opt->value = true;
                } else if ( CaseCompare(val, "no", MAX_VAL_LEN) ) {
                    *(bool *)opt->value = false;
                } else {
                    fprintf(stderr,
                            "expected 'yes' or 'no' for boolean key '%s'\n",
                            key);
                    return false;
                }
                break;

            case CONFIG_OCT_INT:
            case CONFIG_DEC_INT:
            case CONFIG_HEX_INT:
                *(int *)opt->value = (int)strtol(val, NULL, 0);
                break;

            case CONFIG_FLOAT:
                *(float *)opt->value = (float)strtod(val, NULL);
                break;

            case CONFIG_DOUBLE:
                *(double *)opt->value = strtod(val, NULL);
                break;

            case CONFIG_STR: {
                size_t len = strlen(val);
                if ( len > opt->len ) {
                    fprintf(stderr, "value for options %s too long\n", opt->name);
                    return false;
                }

                if ( len < 2 || val[0] != '"' || val[len - 1] != '"' ) {
                    fprintf(stderr,
                            "expected double-quoted string for key '%s'\n",
                            key);
                    return false;
                }
                len -= 2; // without quotes
                strncpy(opt->value, &val[1], len);
                *((char *)opt->value + len) = '\0';
                break;
            }

            case CONFIG_NULL:
            case CONFIG_COMMENT:
            case CONFIG_BLANK_LINE:
                break;
        }
    }

    fclose(file);
    return true;
}

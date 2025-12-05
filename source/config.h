//
//  config.h
//  Config file i/o
//
//  Created by Thomas Foster on 7/17/25.
//

#ifndef config_h
#define config_h

#include <stdbool.h>
#include <stdlib.h>

#define OPT_MISC 100

typedef enum {
    CONFIG_NULL,
    CONFIG_BOOL, // Written as 'yes' or 'no' in a config file.
    CONFIG_STR,  // Double quotes

    // These specify in what format an integer will be written on save.
    CONFIG_OCT_INT, // 01234
    CONFIG_DEC_INT, // 1234
    CONFIG_HEX_INT, // 0x1234
    CONFIG_FLOAT,
    CONFIG_DOUBLE,

    // These are not actually options:
    CONFIG_COMMENT = OPT_MISC,
    CONFIG_BLANK_LINE, // Just separator for formatting a config line.
} OptionType;

// A single option within a config file.
// Define a list of these, terminating with type == CONFIG_NULL.
typedef struct {
    OptionType type;
    const char * name; // Case-insensitive
    void * value;
    size_t len; // For strings
} Option;

typedef bool (*ConfigFunc)(const Option *, const char *);

bool SaveConfig(const Option * options, const char * path);
bool LoadConfig(const Option * options, const char * path);

#endif /* config_h */

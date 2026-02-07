#!/bin/bash
cc source/*.c -lSDL3 \
    -Wall -Wextra -Werror \
    -Wno-missing-field-initializers \
    -Wconversion \
    -o te
cp te ~/bin

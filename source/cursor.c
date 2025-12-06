//
//  cursor.c
//  Cursors for different tools
//
//  Created by Thomas Foster on 10/21/25.
//

#include "cursor.h"
#include "editor.h"

#include <stdio.h>

static SDL_Cursor * cursors[CURSOR_COUNT];
static Cursor current = CURSOR_SYSTEM;

static SDL_Cursor * LoadCursor(const char * base, int hot_x, int hot_y)
{
    char path[256] = { 0 };
    snprintf(path, sizeof(path), "assets/cursors/%s.bmp", base);
    SDL_Surface * surface = SDL_LoadBMP(path);
    if ( surface == NULL ) {
        return NULL;
    }

    SDL_Cursor * cursor = SDL_CreateColorCursor(surface, hot_x, hot_y);
    SDL_DestroySurface(surface);

    return cursor;
}

void InitCursors(void)
{
    cursors[CURSOR_CROSSHAIR] = LoadCursor("cursor", 7, 7);
    cursors[CURSOR_ERASE] = LoadCursor("eraser", 9, 7);
    cursors[CURSOR_DRAG] = LoadCursor("grab_hand", 6, 8);
    cursors[CURSOR_FILL] = LoadCursor("paint_bucket", 3, 14);
    cursors[CURSOR_PAINT] = LoadCursor("paint_brush", 5, 17);
    cursors[CURSOR_SYSTEM] = SDL_GetDefaultCursor();
}

void SetCursor(Cursor new_cursor)
{
    if ( current != new_cursor ) {
        SDL_SetCursor(cursors[new_cursor]);
        current = new_cursor;
    }
}

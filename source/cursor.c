//
//  cursor.c
//  Cursors for different tools
//
//  Created by Thomas Foster on 10/21/25.
//

#include "cursor.h"
#include "editor.h"
#include "cursor_data.h"

#include <stdio.h>

static SDL_Cursor * cursors[CURSOR_COUNT];
static Cursor current = CURSOR_SYSTEM;

static SDL_Cursor * LoadCursor(const unsigned char * data,
                               size_t size,
                               int hot_x,
                               int hot_y)
{
    SDL_IOStream * stream = SDL_IOFromConstMem(data, size);
    SDL_Surface * surface = SDL_LoadBMP_IO(stream, true);
    if ( surface == NULL ) {
        fprintf(stderr, "failed to load cursor\n");
        return NULL;
    }

    SDL_Cursor * cursor = SDL_CreateColorCursor(surface, hot_x, hot_y);
    SDL_DestroySurface(surface);

    return cursor;
}

void InitCursors(void)
{
    cursors[CURSOR_CROSSHAIR] = LoadCursor(cursor_bmp, sizeof(cursor_bmp), 7, 7);
    cursors[CURSOR_ERASE] = LoadCursor(eraser_bmp, sizeof(eraser_bmp), 9, 7);
    cursors[CURSOR_DRAG] = LoadCursor(grab_hand_bmp, sizeof(grab_hand_bmp), 6, 8);
    cursors[CURSOR_FILL] = LoadCursor(paint_bucket_bmp, sizeof(paint_bucket_bmp), 3, 14);
    cursors[CURSOR_PAINT] = LoadCursor(paint_brush_bmp, sizeof(paint_brush_bmp), 5, 17);
    cursors[CURSOR_SYSTEM] = SDL_GetDefaultCursor();
}

void SetCursor(Cursor new_cursor)
{
    if ( current != new_cursor ) {
        SDL_SetCursor(cursors[new_cursor]);
        current = new_cursor;
    }
}

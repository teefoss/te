//
//  cursor.h
//  te
//
//  Created by Thomas Foster on 12/2/25.
//

#ifndef cursor_h
#define cursor_h

typedef enum {
    CURSOR_CROSSHAIR,
    CURSOR_DRAG,
    CURSOR_ERASE,
    CURSOR_FILL,
    CURSOR_PAINT,
    CURSOR_SYSTEM,

    CURSOR_COUNT
} Cursor;

void InitCursors(void);
void SetCursor(Cursor new_cursor);

#endif /* cursor_h */

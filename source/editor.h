//
//  editor.h
//  te
//
//  Created by Thomas Foster on 10/20/25.
//

#ifndef editor_h
#define editor_h

#include "undo.h"
#include "map.h"
#include "view.h"

#include <stdbool.h>
#include <SDL3/SDL.h>

#define MAP_NAME_LEN 128
#define MAX_FLAGS 32
#define BG_GRAY 38
#define LT_GRAY 64
#define BORDER_GRAY 64

// System control key for ctrl-s, ctrl-c, etc.
#ifdef __APPLE__
#define CTRL_KEY (SDL_KMOD_GUI)
#else
#define CTRL_KEY (SDL_KMOD_CTRL)
#endif

#define SWAP(a, b) \
    do { __typeof__(a) _tmp = (a); (a) = (b); (b) = _tmp; } while (0)

typedef struct {
    char name[128];
    bool is_visible;
} EditorLayer;

typedef struct {
    bool (* respond)(const SDL_Event *);
    void (* update)(void);
    void (* render)(void);
} State;

typedef struct editor_map {
    char path[MAP_NAME_LEN];
    Map map;
    View view;

    bool focus_screen;
    int screen_x;
    int screen_y;
    
    bool is_dirty;

    ChangeStack undo;
    ChangeStack redo;

    struct editor_map * next;
} EditorMap;

#endif /* editor_h */

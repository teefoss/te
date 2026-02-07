//
//  map_list.c
//  te
//
//  Created by Thomas Foster on 12/2/25.
//

#include "av.h"
#include "map_list.h"
#include "misc.h"
#include "zoom.h"
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <SDL3/SDL.h>

static EditorMap * map_head; // Singly linked list
static EditorMap * map_tail;

EditorMap * __map; // being edited.
char __current_map_name[MAP_NAME_LEN];

void OpenEditorMap(const char * path, Uint16 width, Uint16 height, Uint8 num_layers)
{
    EditorMap * new_map = SDL_calloc(1, sizeof(EditorMap));
    if ( new_map == NULL ) {
        LogError("could not allocate map");
        exit(EXIT_FAILURE);
    }

    if ( !LoadMap(&new_map->map, path) ) {
        CreateMap(path, width, height, num_layers); // Create the file.
        LoadMap(&new_map->map, path);
    }

    strncpy(new_map->path, path, sizeof(new_map->path));

    // Add to end of map list.
    if ( map_head == NULL ) {
        map_head = new_map;
    } else {
        map_tail->next = new_map;
    }

    map_tail = new_map;
}

void SaveCurrentMap(void)
{
    SaveMap(&__map->map, __map->path);
    __map->is_dirty = false;
}

void MapNextItem(int direction)
{
    if ( RecordingChange() ) {
        return;
    }
    
    SaveCurrentMap();

    if ( direction == 1 && __map->next != NULL ) {
        __map = __map->next;
    } else if ( direction == -1 && __map != map_head) {
        for ( EditorMap * m = map_head; m != NULL; m = m->next ) {
            if ( m->next == __map ) {
                __map = m;
            }
        }
    }

    strncpy(__current_map_name, __map->path, MAP_NAME_LEN);
}

const char * CurrentMapPath(void)
{
    return __current_map_name;
}

void UpdateMapViews(const SDL_Rect * palette_viewport, int font_height, int tile_size)
{
    int ww, wh;
    SDL_GetWindowSize(__window, &ww, &wh);

    const SDL_Rect * vp = palette_viewport;
    for ( EditorMap * m = map_head; m != NULL; m = m->next ) {
        View * v = &m->view;
        v->content_w = m->map.width * tile_size;
        v->content_h = m->map.height * tile_size;
        v->viewport.x = vp->x + vp->w + font_height;
        v->viewport.y = font_height * 2;
        v->viewport.w = wh = ww - (v->viewport.x + font_height);
        v->viewport.h = vp->h;
    }
}

void InitMapViews(void)
{
    for ( EditorMap * m = map_head; m != NULL; m = m->next ) {
        m->view.zoom_index = DefaultZoom();
        m->view.next_item = MapNextItem;
    }
}

void SelectDefaultCurrentMap(void)
{
    __map = map_head;
    strncpy(__current_map_name, __map->path, MAP_NAME_LEN);
}

void SetCurrentMap(const char * path)
{
    for ( EditorMap * m = map_head; m != NULL; m = m->next ) {
        if ( STREQ(m->path, path) ) {
            __map = m;
            strncpy(__current_map_name, __map->path, sizeof(__current_map_name));
            return;
        }
    }
}

static void EditorStatePerform(EditorMap * editor_map,
                               ConfigFunc func)
{
    char * project_path = GetProjectStateDirectory();
    char full_path[1024] = { 0 };
    snprintf(full_path, sizeof(full_path), "%s/%s.txt", project_path, editor_map->path);

    Option options[] = {
        { CONFIG_FLOAT,     "x_position",   &editor_map->view.origin.x },
        { CONFIG_FLOAT,     "y_position",   &editor_map->view.origin.y },
        { CONFIG_DEC_INT,   "screen_x",     &editor_map->screen_x },
        { CONFIG_DEC_INT,   "screen_y",     &editor_map->screen_y },
        { CONFIG_BOOL,      "focus_screen", &editor_map->focus_screen },
        { CONFIG_DEC_INT,   "zoom",         &editor_map->view.zoom_index },
        { CONFIG_NULL },
    };

    func(options, full_path);
}

void LoadMapState(void)
{
    for ( EditorMap * m = map_head; m != NULL; m = m->next ) {
        EditorStatePerform(m, LoadConfig);
    }
}

void SaveMapState(void)
{
    for ( EditorMap * m = map_head; m != NULL; m = m->next ) {
        EditorStatePerform(m, SaveConfig);
    }
}

void FreeMaps(void)
{
    EditorMap * m = map_head;
    while ( m != NULL ) {
        FreeMap(&m->map);
        FreeChangeStack(&m->undo);
        FreeChangeStack(&m->redo);

        EditorMap * temp = m;
        m = m->next;
        SDL_free(temp);
    }
}

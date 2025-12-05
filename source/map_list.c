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
#include <SDL3/SDL.h>

static EditorMap * map_head; // Singly linked list
static EditorMap * map_tail;

EditorMap * map; // being edited.
char current_map_name[MAP_NAME_LEN];

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
    SaveMap(&map->map, map->path);
    map->is_dirty = false;
    SetStatus("Saved '%s'\n", map->path);
}

void MapNextItem(int direction)
{
    SaveCurrentMap();
    CancelChange(map);

    if ( direction == 1 && map->next != NULL ) {
        map = map->next;
    } else if ( direction == -1 && map != map_head) {
        for ( EditorMap * m = map_head; m != NULL; m = m->next ) {
            if ( m->next == map ) {
                map = m;
            }
        }
    }

    strncpy(current_map_name, map->path, MAP_NAME_LEN);
}

const char * CurrentMapPath(void)
{
    return current_map_name;
}

void UpdateMapViews(const SDL_Rect * palette_viewport, int font_height, int tile_size)
{
    int ww, wh;
    SDL_GetWindowSize(window, &ww, &wh);

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
    map = map_head;
    strncpy(current_map_name, map->path, MAP_NAME_LEN);
}

void SetCurrentMap(const char * path)
{
    for ( EditorMap * m = map_head; m != NULL; m = m->next ) {
        if ( STREQ(m->path, path) ) {
            map = m;
            strncpy(current_map_name, map->path, sizeof(current_map_name));
            return;
        }
    }
}

static void EditorStatePerform(EditorMap * map, ConfigFunc func)
{
    if ( !SDL_CreateDirectory(".state") ) {
        LogError("SDL_CreateDirectory failed: %s", SDL_GetError());
        return;
    }

    char path[1024] = { 0 };
    snprintf(path, sizeof(path), ".state/%s_state.txt", map->path);

    Option options[] = {
        { CONFIG_FLOAT,     "x_position",   &map->view.origin.x },
        { CONFIG_FLOAT,     "y_position",   &map->view.origin.y },
        { CONFIG_DEC_INT,   "zoom",         &map->view.zoom_index },
        { CONFIG_NULL },
    };

    func(options, path);
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

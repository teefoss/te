//
//  undo.c
//  te
//
//  Created by Thomas Foster on 11/16/25.
//

#include "editor.h"
#include <limits.h>

static Change current_change; // Current in-progress action
static bool recording = false;
static int saved_redo_top; // When cancelling an action

void BeginChange(EditorMap * map, ChangeType type)
{
    recording = true;
    current_change.type = type;
    saved_redo_top = map->history.redo_top;
    map->history.redo_top = 0;

    switch ( type ) {
        case CHANGE_SET_TILES: {
            TileChanges * changes = &current_change.tile_changes;
            changes->count = 0;
            changes->allocated = 16;
            changes->list = SDL_malloc(changes->allocated * sizeof(TileChange));
            break;
        }
        default:
            break;
    }
}

void CancelChange(EditorMap * map)
{
    if ( recording ) {
        recording = false;
        map->history.redo_top = saved_redo_top;
    }
}

static bool ValidateChange(ChangeType type)
{
    return recording && current_change.type == type;
}

void AddTileChange(int x, int y, int layer, GID old, GID new)
{
    if ( !ValidateChange(CHANGE_SET_TILES) ) return; // TODO: error?
    if ( old == new ) return;

    // Check for duplicate tile in this action
    for ( int i = 0; i < current_change.tile_changes.count; i++ ) {
        TileChange * c = &current_change.tile_changes.list[i];
        if ( c->x == x && c->y == y ) {
            // Update the new value only; keep original old_tile
            c->new = new;
            return;
        }
    }

    // Add new change

    TileChanges * changes = &current_change.tile_changes;

    // Increase list size as needed.
    if ( changes->count >= changes->allocated ) {
        changes->allocated *= 2;
        size_t size = changes->allocated * sizeof(TileChange);
        changes->list = SDL_realloc(changes->list, size);
    }

    TileChange * c = &changes->list[changes->count++];
    c->x = x;
    c->y = y;
    c->layer = layer;
    c->old = old;
    c->new = new;
}

void RegisterMapSizeChange(EditorMap * map, int dx, int dy)
{
    if ( dx == 0 && dy == 0 ) return;

    BeginChange(map, CHANGE_MAP_SIZE);

    int map_w = map->map.width;
    int map_h = map->map.height;

    MapSizeChange * change = &current_change.map_size_changes;;
    change->dx = dx;
    change->dy = dy;

    int save_w;
    int save_h;
    int start_x = 0;
    int start_y = 0;

    if ( dx < 0 ) {
        save_w = -dx;
        save_h = map_h;
        start_x = map_w + dx;
    } else if ( dy < 0 ) {
        save_w = map_w;
        save_h = -dy;
        start_y = map_h + dy;
    } else {
        EndChange(map);
        return;
    }

    change->num_tiles = save_w * save_h;

    size_t size = change->num_tiles * sizeof(*change->tiles);
    change->tiles = SDL_malloc(size);

    Tile * t = change->tiles;
    for ( int l = 0; l < map->map.num_layers; l++ ) {
        for ( int y = start_y; y < map_h; y++ ) {
            for ( int x = start_x; x < map_w; x++ ) {
                t->layer = l;
                t->x = x;
                t->y = y;
                t->gid = GetMapTile(&map->map, x, y, l);
            }
        }
    }

    EndChange(map);
}

void EndChange(EditorMap * map)
{
    if ( !recording ) return;
    recording = false;

    switch ( current_change.type ) {
        case CHANGE_SET_TILES:
            if ( current_change.tile_changes.count == 0 ) {
                printf("  no changes\n");
                return; // No changes
            }
            break;
        default:
            break;
    }

    // Push to undo stack
    History * history = &map->history;
    if ( history->undo_top < MAX_HISTORY ) {
        history->undo_stack[history->undo_top++] = current_change;
    }
}

// TODO: use set tile
void Undo(EditorMap * map)
{
    History * history = &map->history;
    Map * m = &map->map;

    if ( history->undo_top == 0 ) return;

    // Pop action off undo stack
    Change a = history->undo_stack[--history->undo_top];

    switch ( a.type ) {
        case CHANGE_SET_TILES:
            for ( int i = 0; i < a.tile_changes.count; i++ ) {
                TileChange * c = &a.tile_changes.list[i];
                m->tiles[c->layer][c->y * m->width + c->x] = c->old;
            }
            break;

        case CHANGE_MAP_SIZE: {
//            MapSizeChange * c = &a.map_size_changes;
            break;
        }

        default:
            break;
    }

    history->redo_stack[history->redo_top++] = a; // Push
}

void Redo(EditorMap * map)
{
    History * history = &map->history;
    Map * m = &map->map;

    if ( history->redo_top == 0 ) return;

    // Pop action off redo stack
    Change a = history->redo_stack[--history->redo_top];

    switch ( a.type ) {
        case CHANGE_SET_TILES:
            for ( int i = 0; i < a.tile_changes.count; i++ ) {
                TileChange * c = &a.tile_changes.list[i];
                m->tiles[c->layer][c->y * m->width + c->x] = c->new;
            }
            break;
        default:
            break;
    }

    history->undo_stack[history->undo_top++] = a; // Push
}

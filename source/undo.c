//
//  undo.c
//  te
//
//  Created by Thomas Foster on 11/16/25.
//

#include "editor.h"
#include <stdio.h>
#include <limits.h>

static Change current_change; // Current in-progress action
static bool recording = false;

bool RecordingChange(void)
{
    return recording;
}

static Change CopyChange(Change * change)
{
    Change copy = *change;

    switch ( change->type ) {
        case CHANGE_SET_TILES: {
            TileChanges * src = &change->tile_changes;
            TileChanges * dst = &copy.tile_changes;

            size_t size = (size_t)src->count * sizeof(*src->list);
            dst->list = SDL_malloc(size);
            memcpy(dst->list, src->list, size);
            break;
        }
        case CHANGE_MAP_SIZE: {
            MapSizeChange * src = &change->map_size_changes;
            MapSizeChange * dst = &copy.map_size_changes;

            size_t size = (size_t)src->num_tiles * sizeof(*src->tiles);
            dst->tiles = SDL_malloc(size);
            memcpy(dst->tiles, src->tiles, size);
            break;
        }
    }

    return copy;
}

static void FreeChange(Change * change)
{
    switch ( change->type ) {
        case CHANGE_SET_TILES:
            SDL_free(change->tile_changes.list);
            change->tile_changes.list = NULL;
            change->tile_changes.count = 0;
            change->tile_changes.allocated = 0;
            break;
        case CHANGE_MAP_SIZE:
            SDL_free(change->map_size_changes.tiles);
            change->map_size_changes.tiles = NULL;
            change->map_size_changes.num_tiles = 0;
            break;
    }
}

void FreeChangeStack(ChangeStack * stack)
{
    for ( int i = 0; i < stack->count; i++ ) {
        FreeChange(&stack->changes[i]);
    }
    stack->count = 0;
}

static void PushChange(ChangeStack * stack, Change * change)
{
    stack->changes[stack->count++] = CopyChange(change);
}

static Change PopChange(ChangeStack * stack)
{
    Change * top = &stack->changes[stack->count - 1];
    Change change = CopyChange(top);
    FreeChange(top);
    stack->count--;

    return change;
}

void BeginChange(EditorMap * map, ChangeType type)
{
    recording = true;
    current_change.type = type;

    // The redo stack gets cleared when making a new change.
    FreeChangeStack(&map->redo);

    switch ( type ) {
        case CHANGE_SET_TILES: {
            TileChanges * changes = &current_change.tile_changes;

            if ( changes->list ) {
                SDL_free(changes->list);
            }

            changes->count = 0;
            changes->allocated = 16;
            changes->list = SDL_malloc((size_t)changes->allocated * sizeof(TileChange));
            break;
        }

        case CHANGE_MAP_SIZE: {
            break;
        }
        default:
            break;
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
        size_t size = (size_t)changes->allocated * sizeof(TileChange);
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

    MapSizeChange * change = &current_change.map_size_changes;
    change->dx = dx;
    change->dy = dy;

    int save_w;
    int save_h;
    int start_x = 0;
    int start_y = 0;

    // Only reductions in size require saving tiles.
    if ( dx < 0 ) {
        // Save the left edge of the map
        save_w = -dx;
        save_h = map_h;
        start_x = map_w + dx;
    } else if ( dy < 0 ) {
        // Save the bottom edge of the map
        save_w = map_w;
        save_h = -dy;
        start_y = map_h + dy;
    } else {
        EndChange(map);
        return;
    }

    change->num_tiles = save_w * save_h * map->map.num_layers;

    size_t size = (size_t)change->num_tiles * sizeof(*change->tiles);
    if ( change->tiles != NULL ) {
        SDL_free(change->tiles);
    }
    change->tiles = SDL_malloc(size);

    Tile * t = change->tiles;
    for ( int l = 0; l < map->map.num_layers; l++ ) {
        for ( int y = start_y; y < map_h; y++ ) {
            for ( int x = start_x; x < map_w; x++ ) {
                t->layer = l;
                t->x = x;
                t->y = y;
                t->gid = GetMapTile(&map->map, x, y, l);
                t++;
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
        case CHANGE_MAP_SIZE:
            break;
        default:
            break;
    }

    // Push to undo stack
    if ( map->undo.count < MAX_HISTORY ) {
        PushChange(&map->undo, &current_change);
    }

    FreeChange(&current_change);
}

static void RestoreTiles(MapSizeChange * c, Map * m)
{
    for ( int i = 0; i < c->num_tiles; i++ ) {
        Tile * t = &c->tiles[i];
        SetMapTile(m, t->x, t->y, t->layer, t->gid);
    }
}

// TODO: use set tile
void Undo(EditorMap * map)
{
    Map * m = &map->map;

    if ( map->undo.count == 0 ) return;

    // Pop action off undo stack
    Change a = PopChange(&map->undo);

    // Apply change.
    switch ( a.type ) {
        case CHANGE_SET_TILES:
            for ( int i = 0; i < a.tile_changes.count; i++ ) {
                TileChange * c = &a.tile_changes.list[i];
                m->tiles[c->layer][c->y * m->width + c->x] = c->old;
            }
            break;

        case CHANGE_MAP_SIZE: {
            MapSizeChange * c = &a.map_size_changes;

            int restored_w = m->width - c->dx;
            int restored_h = m->height - c->dy;
            ResizeMap(m, (Uint16)restored_w, (Uint16)restored_h);

            if ( c->dx < 0 || c->dy < 0 ) {
                RestoreTiles(c, m);
            }

            break;
        }

        default:
            break;
    }

    if ( map->redo.count < MAX_HISTORY ) {
        PushChange(&map->redo, &a);
        FreeChange(&a);
    }
}

void Redo(EditorMap * map)
{
    Map * m = &map->map;

    if ( map->redo.count == 0 ) return;

    // Pop action off redo stack
    Change a = PopChange(&map->redo);

    switch ( a.type ) {
        case CHANGE_SET_TILES:
            for ( int i = 0; i < a.tile_changes.count; i++ ) {
                TileChange * c = &a.tile_changes.list[i];
                m->tiles[c->layer][c->y * m->width + c->x] = c->new;
            }
            break;

        case CHANGE_MAP_SIZE: {
            MapSizeChange * c = &a.map_size_changes;

            int new_w = m->width + c->dx;
            int new_h = m->height + c->dy;
            ResizeMap(m, (Uint16)new_w, (Uint16)new_h);

            if ( c->dx > 0 || c->dy > 0 ) {
                RestoreTiles(c, m);
            }
            break;
        }

        default:
            break;
    }

//    map->undo.changes[map->undo.count++] = a; // Push
    if ( map->undo.count < MAX_HISTORY ) {
        PushChange(&map->undo, &a);
        FreeChange(&a);
    }
}

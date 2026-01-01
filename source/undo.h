//
//  undo.h
//  te
//
//  Created by Thomas Foster on 11/29/25.
//

#ifndef undo_h
#define undo_h

#include "map.h"

#define MAX_HISTORY 256

typedef struct editor_map EditorMap;

typedef enum {
    CHANGE_SET_TILES, // Paint, fill, paste tiles, etc
    CHANGE_MAP_SIZE,
} ChangeType;

typedef struct {
    int x;
    int y;
    int layer;
    GID old;
    GID new;
} TileChange;

typedef struct {
    int layer;
    int x;
    int y;
    GID gid;
} Tile;

typedef struct {
    int dx;
    int dy;
    Tile * tiles;
    int num_tiles;
} MapSizeChange;

typedef struct {
    TileChange * list;
    int allocated; // Slots allocated.
    int count; // Number of slots in use.
} TileChanges;

typedef struct {
    ChangeType type;
    union {
        TileChanges tile_changes;
        MapSizeChange map_size_changes;
    };
} Change;

typedef struct {
    Change changes[MAX_HISTORY];
    int count;
} ChangeStack;

void BeginChange(EditorMap * map, ChangeType type);
bool RecordingChange(void);
void EndChange(EditorMap * map);
void FreeChangeStack(ChangeStack * stack);

// These are called between a BeginChange and EndChange call:
void AddTileChange(int x, int y, int layer, GID old, GID new);

// These are called on their own and are equivalent to Begin...Add...End:
void RegisterMapSizeChange(EditorMap * map, int dx, int dy);

void Undo(EditorMap * map);
void Redo(EditorMap * map);

#endif /* undo_h */

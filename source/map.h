//
//  map.h
//  te
//
//  Created by Thomas Foster on 11/5/25.
//

#ifndef __map_h
#define __map_h

#include <SDL3/SDL.h>

#define MAX_MAP_WIDTH 0xFFFF
#define MAX_MAP_HEIGHT 0xFFFF
#define MAX_MAPS 256
#define MAX_LAYERS 8
#define MAX_TILESETS 64

typedef Uint16 GID; // Global Tile ID

// Map file layer info table entry: ocation and size of compressed data within
// map file.
typedef struct {
    Uint32 offset;
    Uint32 size;
} LayerInfo;

// At start of map file.
typedef struct {
    Uint16 width;
    Uint16 height;
    Uint8 bg_color[3]; // { R, G, B, unused }
    Uint8 num_layers;
} MapHeader;

typedef struct tileset {
    char id[64];
    GID first_gid;
    int rows;
    int columns;
    int num_tiles;
    int tile_size;
    SDL_Texture * texture;

    struct tileset * prev;
    struct tileset * next;
} Tileset;

typedef struct {
    GID * tiles[MAX_LAYERS];
    Uint16 width;
    Uint16 height;
    Uint8 num_layers;
    SDL_Color bg_color;
} Map;

bool SaveMap(Map * map, const char * path);
bool LoadMap(Map * map, const char * path);
bool CreateMap(const char * path, Uint16 w, Uint16 h, Uint8 num_layers);

bool IsValidPosition(const Map * map, int x, int y);
GID GetMapTile(const Map * map, int x, int y, int layer);
void SetMapTile(Map * map, int x, int y, int layer, GID gid);

// Tilesets

typedef SDL_Texture * (* TilesetTextureLoader)(SDL_Renderer *, const char * id);

void GetTilesetPath(const char * id, char * out, size_t len);
void AddTileset(Tileset ** list, Tileset * tileset);
Tileset * GetGIDLocation(Tileset * tilesets, GID gid, int * x, int * y);

///
/// Load tilesets from project file.
///
/// - parameter texture_loader: The callback used to create the tileset's
///   texture, or NULL to use the default.
///
/// - returns: A linked list of tilesets.
///
Tileset * LoadTilesets(SDL_Renderer * renderer,
                       const char * project_file_path,
                       int tile_size,
                       TilesetTextureLoader texture_loader);

void RenderTile(SDL_Renderer * renderer,
                GID gid,
                Tileset * tilesets,
                const SDL_FRect * dest);

/// Render tile directly from tileset, assuming this is the only tileset in use.
void RenderTile2(SDL_Renderer * renderer,
                 GID gid,
                 SDL_Texture * tileset,
                 int tile_size,
                 const SDL_FRect * dest);

#endif /* __map_h */

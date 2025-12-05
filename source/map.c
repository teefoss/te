//
//  map.c
//  te
//
//  Created by Thomas Foster on 11/5/25.
//

/*
 MAP FORMAT
-----------
 Header             (`MapHeader`)
 Layer Info Table   (`Layer` * num_layers)
 Layer 0 data (compressed)
 Layer 1 data (compressed)
 ...
 */

#include "map.h"

#include <errno.h>
#include <stdlib.h>

#define RLE_TAG 0xABCD

static Uint16 *
Compress(Uint16 * data, size_t data_size, size_t * compressed_size)
{
    if ( data == NULL || data_size == 0 ) {
        *compressed_size = 0;
        return NULL;
    }

    const size_t header_size = sizeof(Uint64);
    Uint16 * buffer = malloc(header_size + data_size);
    if ( buffer == NULL ) {
        return NULL;
    }

    // Write the uncompressed data size at the start.
    *(Uint64 *)buffer = (Uint64)data_size;

    Uint16 * dest_start = buffer + header_size / sizeof(Uint16);
    Uint16 * dest = dest_start;
    Uint16 * source = data;
    Uint16 * source_end = data + (data_size + 1) / sizeof(Uint16);

    while ( source < source_end ) {
        Uint16 count = 1;
        Uint16 value = *source++;

        // Count repeated values.
        while ( source < source_end && *source == value && count < 0xFFFF ) {
            count++;
            source++;
        }

        if ( count > 3 || value == RLE_TAG ) { // Compress
            *dest++ = RLE_TAG;
            *dest++ = count;
            *dest++ = value;
        } else { // Write uncompressed value.
            for ( Uint32 i = 0; i < count; i++ ) {
                *dest++ = value;
            }
        }
    }

    *compressed_size = sizeof(Uint64) + sizeof(Uint16) * (dest - dest_start);

    // Compression didn't save space, just return the original data.
    if ( *compressed_size >= data_size ) {
        *compressed_size = sizeof(Uint64) + data_size;
        memcpy(dest_start, data, data_size);
    }

    return buffer;
}

static Uint16 *
Decompress(Uint16 * data, size_t size, size_t * uncompressed_size)
{
    size_t header_size = sizeof(Uint64);
    *uncompressed_size = *(Uint64 *)data;
    data += header_size / sizeof(Uint16); // Move past the header.

    Uint16 * buffer = malloc(*uncompressed_size);
    if ( buffer == NULL ) {
        return NULL;
    }

    Uint16 * source = data;
    Uint16 * source_end = data + (size - header_size) / sizeof(Uint16);
    Uint16 * dest = buffer;

    while ( source < source_end ) {
        Uint16 count = 1;
        Uint16 value = *source++;

        if ( value == (Uint16)RLE_TAG ) {
            count = *source++;
            value = *source++;
        }

        for ( Uint16 i = 0; i < count; i++ ) {
            *dest++ = value;
        }
    }

    return buffer;
}

void FreeMap(Map * map)
{
    for ( int i = 0; i < map->num_layers; i++ ) {
        free(map->tiles[i]);
    }

    memset(map, 0, sizeof(Map));
}

bool SaveMap(Map * map, const char * path)
{
    FILE * file = fopen(path, "w");
    if ( file == NULL ) {
        fprintf(stderr,
                "%s: failed to create file at path '%s'\n", __func__, path);
        return false;
    }

    // Leave room for the header and info table.
    size_t skip = 0;
    skip += sizeof(MapHeader);
    skip += sizeof(LayerInfo) * map->num_layers;
    fseek(file, skip, SEEK_SET);

    // Write layer data.
    LayerInfo layer_info[MAX_LAYERS] = { 0 };
    size_t original_size = map->width * map->height * sizeof(GID);

    for ( int i = 0; i < map->num_layers; i++ ) {
        size_t compressed_size = 0;
        Uint16 * compressed = Compress(map->tiles[i], original_size, &compressed_size);
        // TODO: error

        layer_info[i].size = (Uint32)compressed_size;
        layer_info[i].offset = (Uint32)ftell(file);
        fwrite(compressed, compressed_size, 1, file);
        // TODO: error

        free(compressed);
    }

    // Write header and table at the beginning.
    rewind(file);
    MapHeader header = {
        .width = map->width,
        .height = map->height,
        .bg_color[0] = map->bg_color.r,
        .bg_color[1] = map->bg_color.g,
        .bg_color[2] = map->bg_color.b,
        .num_layers = map->num_layers,
    };

    fwrite(&header, sizeof(header), 1, file);
    fwrite(layer_info, sizeof(layer_info[0]), map->num_layers, file);
    // TODO: write error

    fclose(file);
    return true;
}

bool LoadMap(Map * map, const char * path)
{
    if ( map == NULL ) {
        // TODO: assert
        fprintf(stderr, "%s: map parameter is NULL\n", __func__);
        return false;
    }

    // Free previously loaded map.
    FreeMap(map);

    FILE * file = fopen(path, "r");
    if ( file == NULL ) {
        // TODO: error
        return false;
    }

    MapHeader header;
    fread(&header, sizeof(header), 1, file);
    // TODO: error

    map->num_layers = header.num_layers;
    map->width = header.width;
    map->height = header.height;
    map->bg_color.r = header.bg_color[0];
    map->bg_color.g = header.bg_color[1];
    map->bg_color.b = header.bg_color[2];
    map->bg_color.a = 255;

    printf("Loading %d x %d map with %d layers\n",
           map->width, map->height, map->num_layers);

    // Read layer info table.
    LayerInfo layer_info[MAX_LAYERS];
    fread(layer_info, sizeof(layer_info[0]), map->num_layers, file);

    // Using the layer info table, Read layer data and decompress.
    for ( int i = 0; i < map->num_layers; i++ ) {
        size_t data_size = layer_info[i].size;
        fseek(file, layer_info[i].offset, SEEK_SET);
        Uint16 * data = malloc(data_size);
        // TODO: error

        fread(data, data_size, 1, file);
        // TODO: error

        size_t decompressed_size = 0;
        map->tiles[i] = Decompress(data, data_size, &decompressed_size);
        if ( map->tiles[i] == NULL ) {
            // TODO: error
        }

        size_t expected_size = map->width * map->height * sizeof(GID);
        if ( decompressed_size != expected_size ) {
            fprintf(stderr, "map size mismatch\n");
            return false;
        }

        free(data);
    }

    return true;
}

bool CreateMap(const char * path, int w, int h, int num_layers)
{
    FILE * file = fopen(path, "rb");
    if ( file != NULL ) {
        fclose(file); // File already exists, do nothing.
        return false;
    }

    if ( num_layers == 0 || num_layers > MAX_LAYERS ) {
        fprintf(stderr, "%s: invalid number of layers (%d)\n",
                __func__, num_layers);
        return false;
    }

    Map map = {
        .width = w,
        .height = h,
        .num_layers = num_layers,
    };

    // TODO: bg_color

    // Init tiles
    for ( int i = 0; i < num_layers; i++ ) {
        map.tiles[i] = calloc(w * h, sizeof(*map.tiles[0]));
        if ( map.tiles[i] == NULL ) {
            fprintf(stderr, "%s: calloc failed: %s\n", __func__, strerror(errno));
            return false;
        }
    }

    SaveMap(&map, path); // Create the file
    FreeMap(&map);

    return true;
}

bool IsValidPosition(const Map * map, int x, int y)
{
    return x >= 0 && y >= 0 && x < map->width && y < map->height;
}

GID GetMapTile(const Map * map, int x, int y, int layer)
{
    if ( !IsValidPosition(map, x, y) ) {
        fprintf(stderr, "invalid map position\n");
        return 0;
    }

    return map->tiles[layer][y * map->width + x];
}

void SetMapTile(Map * map, int x, int y, int layer, GID gid)
{
    if ( !IsValidPosition(map, x, y) ) {
        fprintf(stderr, "invalid map position\n");
        return;
    }

    map->tiles[layer][y * map->width + x] = gid;
}

void GetTilesetPath(const char * id, char * out, size_t len)
{
    if ( len == 0 ) return;

    snprintf(out, len, "assets/tilesets/%s.bmp", id);
}

static SDL_Texture *
DefaultTextureLoader(SDL_Renderer * renderer, const char * id)
{
    char full_path[128];
    GetTilesetPath(id, full_path, sizeof(full_path));

    SDL_Texture * texture = NULL;
    SDL_Surface * surface = SDL_LoadBMP(full_path);

    if ( surface != NULL ) {
        texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_DestroySurface(surface);
    }

    return texture;
}

void AddTileset(Tileset ** list, Tileset * tileset)
{
    tileset->next = NULL;
    tileset->prev = NULL;

    if ( *list == NULL ) {
        tileset->first_gid = 1;
        *list = tileset;
        return;
    }

    // Find the last set in the list.
    Tileset * tail = *list;
    while ( tail->next != NULL ) {
        tail = tail->next;
    }

    tileset->prev = tail;
    tail->next = tileset;
    tileset->first_gid = tail->first_gid + tail->num_tiles;
}

Tileset * LoadTilesets(SDL_Renderer * renderer,
                       const char * project_path,
                       int tile_size,
                       TilesetTextureLoader texture_loader)
{
    FILE * file = fopen(project_path, "r");
    if ( file == NULL ) {
        return NULL;
    }

    char line[256] = { 0 };
    Tileset * list = NULL;

    while ( fgets(line, sizeof(line), file) ) {
        Tileset * ts = calloc(1, sizeof(Tileset));
        if ( ts == NULL ) {
            fprintf(stderr, "%s: calloc failed: %s\n",
                    __func__, strerror(errno));
            return NULL;
        }

        if ( sscanf(line, "tile_set: \"%s\"\n", ts->id) == 1 ) {
            if ( texture_loader == NULL ) {
                texture_loader = DefaultTextureLoader;
            }

            ts->texture = texture_loader(renderer, ts->id);
            if ( ts->texture == NULL ) {
                fprintf(stderr, "%s: could not load tileset %s\n",
                        __func__, ts->id);
                return 0;
            }

            ts->rows = ts->texture->w / tile_size;
            ts->columns = ts->texture->h / tile_size;
            ts->num_tiles = ts->rows * ts->columns;
            ts->tile_size = tile_size;
            AddTileset(&list, ts);
        }
    }

    return list;
}

Tileset * GetGIDLocation(Tileset * tilesets, GID gid, int * x, int * y)
{
    Tileset * tail = tilesets;
    while ( tail->next != NULL ) {
        tail = tail->next;
    }

    // Find which tile set this gid belongs to.
    Tileset * ts = tail;
    for ( ; ts->prev != NULL; ts = ts->prev ) {
        if ( gid >= ts->first_gid ) {
            break;
        }
    }

    int index = gid - ts->first_gid;
    *x = index % ts->columns;
    *y = index / ts->rows;
    return ts;
}

void RenderTile(SDL_Renderer * renderer,
                GID gid,
                Tileset * tilesets,
                const SDL_FRect * dest)
{
    int x, y;
    Tileset * ts = GetGIDLocation(tilesets, gid, &x, &y);

    SDL_FRect source = {
        x * ts->tile_size,
        y * ts->tile_size,
        ts->tile_size,
        ts->tile_size
    };

    SDL_RenderTexture(renderer, ts->texture, &source, dest);
}

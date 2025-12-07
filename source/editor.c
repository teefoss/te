//
//  editor.c
//  te
//
//  Created by Thomas Foster on 10/20/25.
//

/*
 -------------------------------------------------------------------------------
 TODO:

    High Priority
    -------------
 -  Tools
    - Rect
 -  Change map size
 -  Build in assets

    Low Priority
    ------------
 -  Backup system: save maps on launch to .backups/yymmdd-hhmmss/
 -  Flags
 -  Obscure all but the active screen
 -  Copy-paste Flip H and/or V
 -  Reading proj file and define an order for each tile_set, map, etc:
    set index with value specified, and append index to order[], inc count
 -  Swap grid lines in front/behind
 -  Swap grid lines in front/behind

    Niceties
    --------
 -  Define app accent color

 -------------------------------------------------------------------------------
 BUGS
 -  View not clamped on init and some actions (resize map)
 -  Control-S not registering on first attempt? Might be just my keyboard

 */

#include "args.h"
#include "asset_data.h"
#include "av.h"
#include "config.h"
#include "cursor.h"
#include "editor.h"
#include "map_list.h"
#include "misc.h"
#include "parser.h"
#include "view.h"
#include "zoom.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <SDL3/SDL.h>

#define DEFAULT_PROJECT_FILE "main.teproj"
#define EDITOR_STATE_CONFIG_FILE ".state/editor_state.txt"
#define MAP_VERSION 1
#define PROJECT_VERSION 1

#define GRAPHICS_SCALE 1
#define TOOL_W 24
#define TOOL_H 24
#define STR_LEN 128
#define PAL_WIDTH_STEP 32
#define STATUS_LEN 64

#define FOR_EACH_TILESET(iter) \
    for (Tileset * iter = _tilesets; iter != NULL; iter = iter->next)

typedef enum {
    GRID_HIDDEN,
    GRID_BACK,
    GRID_FRONT,
} GridSetting;

typedef struct {
    char name[128];
    char ident[128];
} Flag;

typedef struct {
    int width;
    int height;
    GID * tiles[MAX_LAYERS];
    int copied_layers[MAX_LAYERS];
    int num_copied_layers;
    int allocated_slots;
} Clipboard;

#ifdef __APPLE__
#pragma mark - PROTOTYPES
#endif

static bool S_DragMap_Respond(const SDL_Event *);
static void S_DragMap_Update(void);

// Update shared by various dragging states
static void S_Drag_Update(void);

static bool S_DragSelection_Respond(const SDL_Event *);
static void S_DragSelection_Render(void);

static bool S_DragLine_Respond(const SDL_Event *);
static void S_DragLine_Render(void);

static bool S_Main_Respond(const SDL_Event * event);
static void S_Main_Update(void);
static void S_Main_Render(void);

void MapNextItem(int direction);

#ifdef __APPLE__
#pragma mark - DATA
#endif

// Holy global state, batman

static Font *       _font;
static int          _tile_size = 16; // Tile size in pixels.
static int          _screen_w = 0; // Visible region in tiles or 0 if unused.
static int          _screen_h = 0;
static EditorLayer  _layers[MAX_LAYERS];
static int          _num_layers;
static int          _num_flags;
static Flag         _flags[MAX_FLAGS];
static SDL_Color    _bg_color = { 0x00, 0x00, 0x00, 0xFF };
static int          _layer = 0; // Current layer being edited
static Tool         _tool = TOOL_PAINT; // Current tool.
static SDL_FPoint   _prev_mouse; // Location of mouse last frame.
static int          _hover_tile_x;
static int          _hover_tile_y;
static int          _pal_width = 512;
static char         _status[STATUS_LEN];
static float        _status_timer;
static SDL_Rect     _window_frame;
static float        _ants_phase;
static Clipboard    _clipboard;
static bool         _show_clipboard;
static bool         _painting;
static bool         _show_screen_lines;
static bool         _show_grid_lines = true;

// Tilesets
static Tileset *    _tilesets; // Linked list
static Tileset *    _active_tileset; // Tileset currently being displayed
static int          _tile_set_index; // "Index" of active tileset
static int          _num_tilesets;
static View         _tileset_views[MAX_TILESETS];

// Default values for new maps, possibly loaded from project file.
static int          _default_num_layers = 3;
static int          _default_map_width = 128;
static int          _default_map_height = 128;
static SDL_Color    _default_bg_color; // TODO: why this and _bg_color?

// Selection box
static int          _fixed_x; // Start tile of drag box
static int          _fixed_y;
static int          _drag_x; // Current drag tile
static int          _drag_y;

#ifdef __APPLE__
#pragma mark - STATE
#endif

State * _state;

static State drag_map_state = {
    .respond = S_DragMap_Respond,
    .update = S_DragMap_Update,
    .render = S_Main_Render,
};

static State main_state = {
    .respond = S_Main_Respond,
    .update = S_Main_Update,
    .render = S_Main_Render,
};

static State drag_selection_state = {
    .respond = S_DragSelection_Respond,
    .update = S_Drag_Update,
    .render = S_DragSelection_Render,
};

static State drag_line_state = {
    .respond = S_DragLine_Respond,
    .update = S_Drag_Update,
    .render = S_DragLine_Render,
};

// Config (saved editor state)

static Option config[] = {
    { CONFIG_COMMENT,   "" },
    { CONFIG_COMMENT,   "Editor State File - N.B. te creates and reads this file, " },
    { CONFIG_COMMENT,   "do not alter it unless you know what you're doing." },
    { CONFIG_COMMENT,   "" },
    { CONFIG_DEC_INT,   "current_tile_set", &_tile_set_index },
    { CONFIG_DEC_INT,   "palette_zoom_0",   &_tileset_views[0].zoom_index },
    { CONFIG_DEC_INT,   "palette_zoom_1",   &_tileset_views[1].zoom_index },
    { CONFIG_DEC_INT,   "palette_zoom_2",   &_tileset_views[2].zoom_index },
    { CONFIG_DEC_INT,   "palette_zoom_3",   &_tileset_views[3].zoom_index },
    { CONFIG_DEC_INT,   "palette_zoom_4",   &_tileset_views[4].zoom_index },
    { CONFIG_DEC_INT,   "palette_zoom_5",   &_tileset_views[5].zoom_index },
    { CONFIG_DEC_INT,   "palette_zoom_6",   &_tileset_views[6].zoom_index },
    { CONFIG_DEC_INT,   "palette_zoom_7",   &_tileset_views[7].zoom_index },
    { CONFIG_DEC_INT,   "layer",            &_layer },
    { CONFIG_BOOL,      "layer_0_visible",  &_layers[0].is_visible },
    { CONFIG_BOOL,      "layer_1_visible",  &_layers[1].is_visible },
    { CONFIG_BOOL,      "layer_2_visible",  &_layers[2].is_visible },
    { CONFIG_BOOL,      "layer_3_visible",  &_layers[3].is_visible },
    { CONFIG_BOOL,      "layer_4_visible",  &_layers[4].is_visible },
    { CONFIG_BOOL,      "layer_5_visible",  &_layers[5].is_visible },
    { CONFIG_BOOL,      "layer_6_visible",  &_layers[6].is_visible },
    { CONFIG_BOOL,      "layer_7_visible",  &_layers[7].is_visible },
    { CONFIG_DEC_INT,   "window_x",         &_window_frame.x },
    { CONFIG_DEC_INT,   "window_y",         &_window_frame.y },
    { CONFIG_DEC_INT,   "window_w",         &_window_frame.w },
    { CONFIG_DEC_INT,   "window_h",         &_window_frame.h },
    { CONFIG_DEC_INT,   "palette_width",    &_pal_width },
    { CONFIG_BOOL,      "show_grid_lines",  &_show_grid_lines },
    { CONFIG_BOOL,      "show_screen_lines",&_show_screen_lines },
    { CONFIG_STR,       "current_map",      current_map_name, MAP_NAME_LEN },
    { CONFIG_NULL },
};

static const char * tool_names[TOOL_COUNT] = {
    [TOOL_PAINT] = "Paint",
    [TOOL_ERASE] = "Erase",
    [TOOL_FILL] = "Fill",
    [TOOL_LINE] = "Line",
    [TOOL_RECT] = "Rect",
};

#ifdef __APPLE__
#pragma mark - FUNCTIONS
#endif

void SetStatus(const char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(_status, STATUS_LEN, fmt, args);
    va_end(args);

    _status_timer = 3.0f;
}

View * MouseView(void)
{
    float mx, my;
    SDL_GetMouseState(&mx, &my);
    SDL_Point mouse = { (int)mx, (int)my };

    if ( SDL_PointInRect(&mouse, &map->view.viewport) ) {
        return &map->view;
    } else if ( SDL_PointInRect(&mouse, &_tileset_views[0].viewport) ) {
        return &_tileset_views[_tile_set_index];
    }

    return NULL;
}

static void CopyToClipboard(void)
{
    if ( !map->view.has_selection ) {
        return;
    }

    SelectionBox * box = &map->view.selection_box;
    _clipboard.width  = box->max_x - box->min_x + 1;
    _clipboard.height = box->max_y - box->min_y + 1;

    // Resize clipboard if needed.
    int slots_needed = _clipboard.width * _clipboard.height;
    if ( _clipboard.allocated_slots < slots_needed ) {
        size_t size = (size_t)slots_needed * sizeof(*_clipboard.tiles[0]);
        for ( int i = 0; i < map->map.num_layers; i++ ) {
            _clipboard.tiles[i] = realloc(_clipboard.tiles[i], size);
        }
        _clipboard.allocated_slots = slots_needed;
    }

    _clipboard.num_copied_layers = 0;

    for ( int i = 0; i < map->map.num_layers; i++ ) {
        if ( !_layers[i].is_visible ) continue;

        _clipboard.copied_layers[_clipboard.num_copied_layers++] = i;

        for ( int y = 0; y < _clipboard.height; y++ ) {
            for ( int x = 0; x < _clipboard.width; x++ ) {
                GID * dest = &_clipboard.tiles[i][y * _clipboard.width + x];
                int src_x = box->min_x + x;
                int src_y = box->min_y + y;
                *dest = GetMapTile(&map->map, src_x, src_y, i);
            }
        }
    }
}

static void UpdateWindowFrame(void)
{
    SDL_GetWindowSize(window, &_window_frame.w, &_window_frame.h);
    SDL_GetWindowPosition(window, &_window_frame.x, &_window_frame.y);
}

static View * KeyRespondingView(void)
{
    SDL_Keymod mods = SDL_GetModState();
    if ( mods & SDL_KMOD_SHIFT ) {
        return &_tileset_views[_tile_set_index];
    } else {
        return &map->view;
    }
}

int UIMargin(void)
{
    if ( _font == NULL ) {
        fprintf(stderr, "Error: called %s before initializing font!\n", __func__);
        exit(EXIT_FAILURE);
    }

    return FontHeight(_font);
}

SelectionBox * CurrentBrush(void)
{
    return &_tileset_views[_tile_set_index].selection_box;
}

/// Update viewport and content sizes (e.g. after a window size change))
static void UpdateViewSizes(void)
{
    int ww, wh;
    SDL_GetWindowSize(window, &ww, &wh);

    int text_margin = FontHeight(_font) * 2;

    SDL_Rect * vp = &_tileset_views[0].viewport;

    for ( int i = 0; i < _num_tilesets; i++ ) {
        vp = &_tileset_views[i].viewport;
        vp->w = _pal_width;
        vp->h = wh - text_margin * 2;
    }

    UpdateMapViews(vp, FontHeight(_font), _tile_size);
}

void PaletteNextItem(int direction)
{
    if ( direction == -1 && _active_tileset->prev != NULL ) {
        _active_tileset = _active_tileset->prev;
        _tile_set_index--;
    } else if ( direction == 1 && _active_tileset->next != NULL ) {
        _active_tileset = _active_tileset->next;
        _tile_set_index++;
    }
}

static void InitViews(void)
{
    SDL_Rect * vp = &_tileset_views[0].viewport;
    Tileset * ts = _tilesets;

    for ( int i = 0; i < _num_tilesets; i++ ) {
        View * v = &_tileset_views[i];
        v->content_w = ts->texture->w;
        v->content_h = ts->texture->h;
        v->zoom_index = DefaultZoom();
        v->next_item = PaletteNextItem;

        // Viewport
        vp = &_tileset_views[i].viewport;
        vp->x = FontHeight(_font);
        vp->y = FontHeight(_font) * 2;

        ts = ts->next;
    }

    InitMapViews();

    UpdateViewSizes();
}

static SDL_Color Color24ToSDL(int color24)
{
    SDL_Color color;
    color.r = (Uint8)((color24 & 0xFF0000) >> 16);
    color.g = (Uint8)((color24 & 0x00FF00) >> 8);
    color.b = (Uint8)((color24 & 0x0000FF));
    color.a = 255;

    return color;
}

void ReadProjectFile(void)
{
    char * project_path = GetStrOption("--project", "-p");
    if ( project_path == NULL ) {
        project_path = DEFAULT_PROJECT_FILE;
    }

    printf("Loading project '%s'\n...", project_path);
    if ( !BeginParsing(project_path) ) {
        return; // Project file not present.
    }

    char ident[STR_LEN];

    MatchIdent("version");
    MatchSymbol(':');
    int version = ExpectInt();
    if ( version != PROJECT_VERSION ) {
        // TODO: handle version mismatch
    }

    int max_layer_index = 0;

    while ( AcceptIdent(ident, sizeof(ident)) ) {
        if ( STREQ(ident, "tile_size") ) {
            MatchSymbol(':');
            _tile_size = ExpectInt();
            // TODO: validate
        }
        else if ( STREQ(ident, "screen_size") ) {
            MatchSymbol(':');
            _screen_w = ExpectInt();
            _screen_h = ExpectInt();
        }
        else if ( STREQ(ident, "flag") ) {
            int bit = ExpectInt();
            MatchSymbol(':');
            ExpectString(_flags[bit].name, sizeof(_flags[0].name));
            AcceptIdent(_flags[bit].ident, sizeof(_flags[0].ident));
            _num_flags++;
        }
        else if ( STREQ(ident, "tile_set") ) {
            MatchSymbol(':');

            Tileset * set = calloc(1, sizeof(*set));
            ExpectString(set->id, sizeof(set->id));

            char path[256] = { 0 };
            GetTilesetPath(set->id, path, sizeof(path));

            set->texture = LoadTextureFromBMP(path);
            set->rows = set->texture->h / _tile_size;
            set->columns = set->texture->w / _tile_size;
            set->num_tiles = set->rows * set->columns;
            set->tile_size = _tile_size;
            AddTileset(&_tilesets, set);
            _num_tilesets++;
        }
        else if ( STREQ(ident, "background_color") ) {
            MatchSymbol(':');
            int bg24 = ExpectInt();
            _default_bg_color = Color24ToSDL(bg24);
        }
        else if ( STREQ(ident, "layer") ) {
            int layer_num = ExpectInt();
            MatchSymbol(':');
            ExpectString(_layers[layer_num].name, sizeof(_layers[0].name));
            if ( layer_num > max_layer_index ) {
                max_layer_index = layer_num;
                _num_layers = layer_num + 1;
            }
        }
        else if ( STREQ(ident, "layers") ) {
            MatchSymbol(':');
            _default_num_layers = ExpectInt();
        }
        else if ( STREQ(ident, "default_map_size") ) {
            MatchSymbol(':');
            _default_map_width = ExpectInt();
            _default_map_height = ExpectInt();
        } else if ( STREQ(ident, "map") ) {
            MatchSymbol(':');
            char path[MAP_NAME_LEN] = { 0 };
            ExpectIdent(path, sizeof(path));
            strcat(path, ".temap");

            int w = _default_map_width;
            int h = _default_map_height;
            if ( AcceptInt(&w) ) {
                h = ExpectInt();
            }

            OpenEditorMap(path, (Uint16)w, (Uint16)h, (Uint8)_num_layers);
        } else {
            fprintf(stderr, "Unknown property in '%s': '%s'\n", ident, project_path);
            exit(EXIT_FAILURE);
        }
    }

    EndParsing();

    // Now that all _tilesets has been loaded, set _active_tileset using the
    // saved index value from the config:
    int i = 0;
    FOR_EACH_TILESET(set) {
        if ( i == _tile_set_index ) {
            _active_tileset = set;
            break;
        }
        i++;
    }
}

int StringToInt(const char * str, const char * err_str)
{
    int n = (int)strtol(str, NULL, 10);
    if ( n == 0 && (errno == EINVAL || errno == ERANGE) ) {
        fprintf(stderr, "%s\n", err_str);
        exit(EXIT_FAILURE);
    }

    return n;
}

static void InitEditor(void)
{
    // Set default layer names and visibility.
    for ( int i = 0; i < MAX_LAYERS; i++ ) {
        snprintf(_layers[i].name, sizeof(_layers[i].name), "L%d", i);
        _layers[i].is_visible = true;
    }

    ReadProjectFile();

//    _font = LoadFont("editor_assets/04b03.bmp", 8, 8);
    _font = LoadFontFromData(__04b03_bmp, __04b03_bmp_len, 8, 8);
    _font->scale = 3;

    InitCursors();
    InitViews();

    // Load Tile Indicator for current tile size.

    char indicator_path[256] = { 0 };
    snprintf(indicator_path, sizeof(indicator_path),
             "assets/indicator%d.bmp", _tile_size);

    // Load Config or default values.

    SelectDefaultCurrentMap();

    if ( LoadConfig(config, EDITOR_STATE_CONFIG_FILE) ) {
        SDL_SetWindowPosition(window, _window_frame.x, _window_frame.y);
        SDL_SetWindowSize(window, _window_frame.w, _window_frame.h);

        SetCurrentMap(current_map_name);
    } else {
        // Update window frame var with whatever the default is for now.
        UpdateWindowFrame();
    }

    LoadMapState();

    _state = &main_state;

    ClampViewOrigin(&_tileset_views[_tile_set_index]);
}

GID GetTileSetGID(int x, int y)
{
    GID index = (GID)(y * _active_tileset->columns + x);
    return _active_tileset->first_gid + index;
}

void ApplyBrush(void)
{
    SelectionBox * brush = CurrentBrush();
    Map * m = &map->map;

    int src_y = brush->min_y;
    int dst_y = _hover_tile_y;
    for ( ; src_y <= brush->max_y; src_y++, dst_y++ ) {
        if ( src_y >= m->height ) break;

        int src_x = brush->min_x;
        int dst_x = _hover_tile_x;
        for ( ; src_x <= brush->max_x; src_x++, dst_x++ ) {
            if ( src_x >= m->width ) break;

            Uint16 dst_index = (Uint16)(dst_y * m->width + dst_x);
            Uint16 gid = GetTileSetGID(src_x, src_y);
            if ( m->tiles[_layer][dst_index] != gid ) {
                GID old = m->tiles[_layer][dst_index];
                m->tiles[_layer][dst_index] = gid;
                AddTileChange(dst_x, dst_y, _layer, old, gid);
                map->is_dirty = true;
            }
        }
    }
}

static SDL_FRect GetTileUnionRect(const View * view, int x1, int y1, int x2, int y2)
{
    SDL_FRect t1 = GetTileRect(view, x1, y1, _tile_size);
    SDL_FRect t2 = GetTileRect(view, x2, y2, _tile_size);
    SDL_FRect rect;
    SDL_GetRectUnionFloat(&t1, &t2, &rect);

    return rect;
}

static void RenderSelectionBox(const View * view, int tx1, int ty1, int tx2, int ty2)
{
    SDL_Rect old_vp;
    SDL_GetRenderViewport(renderer, &old_vp);
    SDL_SetRenderViewport(renderer, &view->viewport);

    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);

    SDL_FRect br_rect = GetTileUnionRect(view, tx1, ty1, tx2, ty2);

    float zoom = GetScale(view->zoom_index);
    float thickness = zoom;
    float dash_len = (_tile_size / 3) * zoom;
    float dash_gap = dash_len;
    DrawDashedRect(br_rect.x, br_rect.y, br_rect.w, br_rect.h,
                   dash_len, dash_gap, thickness, _ants_phase);

    SDL_SetRenderViewport(renderer, &old_vp);
}

// TODO: this should be a part of the brush, etc
// Probably abstract to 'render tile with background' that can be used by render brush and render clipboard etc
void RenderIndicator(const View * view)
{
    int tx, ty;
    if ( GetMouseTile(view, &tx, &ty, _tile_size) ) {
        SDL_FRect r = GetTileRect(view, tx, ty, _tile_size);

        if ( view == &map->view ) {
            if ( _show_clipboard ) {
                r.w *= _clipboard.width;
                r.h *= _clipboard.height;
            } else {
                SelectionBox * br = CurrentBrush();
                if ( _tool != TOOL_FILL ) {
                    r.w *= (br->max_x - br->min_x) + 1;
                    r.h *= (br->max_y - br->min_y) + 1;
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 32);
        SDL_RenderFillRect(renderer, &r);
    }
}

static void RenderBrush(void)
{
    SelectionBox * brush = CurrentBrush();

    int w = (brush->max_x - brush->min_x) + 1;
    int h = (brush->max_y - brush->min_y) + 1;

    if ( _tool == TOOL_FILL ) {
        w = 1;
        h = 1;
    }

    SDL_FRect src = {
        .x = (float)(brush->min_x * _tile_size),
        .y = (float)(brush->min_y * _tile_size),
        .w = (float)(w * _tile_size),
        .h = (float)(h * _tile_size)
    };

    float scale = GetScale(map->view.zoom_index);
    SDL_FRect dst = {
        _hover_tile_x * _tile_size * scale,
        _hover_tile_y * _tile_size * scale,
        src.w * scale,
        src.h * scale
    };

    dst.x -= map->view.origin.x * scale;
    dst.y -= map->view.origin.y * scale;

    SDL_RenderTexture(renderer, _active_tileset->texture, &src, &dst);
}

static void RenderClipboard(void)
{
    View * v = MouseView();
    if ( v == NULL || v != &map->view ) {
        return;
    }

    Clipboard * cb = &_clipboard;

    for ( int n = 0; n < cb->num_copied_layers; n++ ) {
        int l = cb->copied_layers[n];

        for ( int y = 0; y < cb->height; y++ ) {
            for ( int x = 0; x < cb->width; x++ ) {
                GID gid = cb->tiles[l][y * cb->width + x];
                int dest_x = _hover_tile_x + x;
                int dest_y = _hover_tile_y + y;
                SDL_FRect dest = GetTileRect(&map->view,
                                             dest_x,
                                             dest_y,
                                             _tile_size);
                RenderTile(renderer, gid, _tilesets, &dest);
            }
        }
    }
}

void RenderBorder(SDL_Rect r)
{
    SDL_FRect border = {
        (float)(r.x - 1),
        (float)(r.y - 1),
        (float)(r.w + 2),
        (float)(r.h + 2)
    };
    Uint8 gray = LT_GRAY;
    SDL_SetRenderDrawColor(renderer, gray, gray, gray, 255);
    SDL_RenderRect(renderer, &border);
    border.x -= 1.0f;
    border.y -= 1.0f;
    border.w += 2.0f;
    border.h += 2.0f;
    SDL_RenderRect(renderer, &border);
}

void RenderMapView(void)
{
    View * map_view = &map->view;
    RenderBorder(map_view->viewport);

    SDL_SetRenderViewport(renderer, &map_view->viewport);

    // Background gray
    Uint8 gray = LT_GRAY;
    SDL_SetRenderDrawColor(renderer, gray, gray, gray, 255);
    SDL_RenderFillRect(renderer, NULL);

    RenderViewBackground(map_view, map->map.bg_color);

    if ( _show_grid_lines ) {
        RenderGrid(map_view, map->map.bg_color, 0.2f, _tile_size, _tile_size);
    }

    if ( _show_screen_lines ) {
        RenderGrid(map_view, map->map.bg_color, 0.35f, _tile_size * _screen_w, _tile_size * _screen_h);
    }

    // Render Map
    // TODO: render only visible tiles

    for ( int l = 0; l < map->map.num_layers; l++ ) {
        if ( !_layers[l].is_visible ) {
            continue;
        }

        for ( int y = 0; y < map->map.height; y++ ) {
            // TODO: if l is hidden continue
            for ( int x = 0; x < map->map.width; x++ ) {
                Uint16 gid = map->map.tiles[l][y * map->map.width + x];
                if ( gid != 0 ) {
                    SDL_FRect dest = GetTileRect(map_view, x, y, _tile_size);
                    RenderTile(renderer, gid, _tilesets, &dest);
                }
            }
        }
    }

    SDL_SetRenderViewport(renderer, NULL);
}

/// Render things like the tile indicator, selection, etc.
void RenderViewOverlays(void)
{

}

bool S_DragMap_Respond(const SDL_Event * event)
{
    switch ( event->type ) {
        case SDL_EVENT_KEY_UP:
            if ( event->key.key == SDLK_SPACE ) {
                _state = &main_state;
                return true;
            }
            return false;
        case SDL_EVENT_MOUSE_BUTTON_UP:
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if ( event->button.button == SDL_BUTTON_LEFT ) {
                SDL_GetMouseState(&_prev_mouse.x, &_prev_mouse.y);
                return true;
            }
            return false;
        default:
            return false;
    }
}

// TODO: move to view.c
void UpdateDragView(View * v)
{
    SDL_FPoint now;
    SDL_MouseButtonFlags buttons = SDL_GetMouseState(&now.x, &now.y);
    bool left_held = buttons * SDL_BUTTON_MASK(SDL_BUTTON_LEFT);

    SDL_FRect vp = RectIntToFloat(&v->viewport);
    if ( SDL_PointInRectFloat(&now, &vp) ) {
        SetCursor(CURSOR_DRAG);

        if ( left_held ) {
            v->origin.x -= now.x - _prev_mouse.x;
            v->origin.y -= now.y - _prev_mouse.y;
            _prev_mouse = now;
            ClampViewOrigin(v);
        }
    }
}

void S_DragMap_Update(void)
{
    UpdateDragView(&map->view);
}

void BresenhamLine(int x1,
                   int y1,
                   int x2,
                   int y2,
                   void (* callback)(int x, int y, void * user),
                   void * user)
{
    int dx = abs(x2 - x1);
    int dy = -abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx + dy;
    int e2;

    int x = x1;
    int y = y1;

    callback(x, y, user);

    while ( x != x2 || y != y2 ) {
        e2 = 2 * err;

        if ( e2 >= dy ) {
            err += dy;
            x += sx;
        }

        if ( e2 <= dx ) {
            err += dx;
            y += sy;
        }

        callback(x, y, user);
    }
}

void LineAction_SetTile(int x, int y, void * user)
{
    GID old = GetMapTile(&map->map, x, y, _layer);
    GID new = *(GID *)user;
    SetMapTile(&map->map, x, y, _layer, new);
    AddTileChange(x, y, _layer, old, new);
}

static bool S_DragLine_Respond(const SDL_Event * event)
{
    View * v = MouseView();

    int tx, ty;
    if ( !GetMouseTile(v, &tx, &ty, _tile_size) ) {
        return false; // Mouse inside view but outside tile area.
    }

    switch ( event->type ) {
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            SelectionBox * br = CurrentBrush(); // TODO: GetBrushTile and replace other instances of this repeated code.
            GID tile = GetTileSetGID(br->min_x, br->min_y);
            BresenhamLine(_fixed_x, _fixed_y, _drag_x, _drag_y, LineAction_SetTile, &tile);
            EndChange(map);
            _state = &main_state;
            return true;
        }

        default:
            break;
    }

    return false;
}

static bool S_DragSelection_Respond(const SDL_Event * event)
{
    View * v = MouseView();

    int tx, ty;
    if ( !GetMouseTile(v, &tx, &ty, _tile_size) ) {
        return false; // Mouse inside view but outside tile area.
    }

    switch ( event->type ) {
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            SelectionBox * sel = &v->selection_box;
            sel->min_x = _fixed_x;
            sel->min_y = _fixed_y;
            sel->max_x = _drag_x;
            sel->max_y = _drag_y;

            if ( sel->max_x < sel->min_x ) SWAP(sel->max_x, sel->min_x);
            if ( sel->max_y < sel->min_y ) SWAP(sel->max_y, sel->min_y);

            v->has_selection = true;
            _state = &main_state;
            return true;
        }

        default:
            break;
    }

    return false;
}

void ApplyClipboard(void)
{
    if ( !_show_clipboard ) return;

    View * v = MouseView();
    if ( v == NULL || v != &map->view ) return;

    Clipboard * cb = &_clipboard;
    BeginChange(map, CHANGE_SET_TILES);

    for ( int n = 0; n < cb->num_copied_layers; n++ ) {
        int l = cb->copied_layers[n];

        for ( int y = 0; y < cb->height; y++ ) {
            int dest_y = _hover_tile_y + y;
            if ( dest_y >= map->map.height ) continue;

            for ( int x = 0; x < cb->width; x++ ) {
                int dest_x = _hover_tile_x + x;
                if ( dest_x >= map->map.width ) continue;

                // TODO: convert to get/set tile
                GID * dst = &map->map.tiles[l][dest_y * map->map.width + dest_x];
                GID * src = &cb->tiles[l][y * cb->width + x];
                AddTileChange(dest_x, dest_y, _layer, *dst, *src);
                *dst = *src;
                map->is_dirty = true;
            }
        }
    }

    EndChange(map);
}

void FloodFill_r(int x, int y, GID old, GID new)
{
    if ( !IsValidPosition(&map->map, x, y) ) {
        return;
    }

    GID tile = GetMapTile(&map->map, x, y, _layer);
    if ( tile != old ) {
        return;
    }

    SetMapTile(&map->map, x, y, _layer, new);
    AddTileChange(x, y, _layer, old, new);

    FloodFill_r(x + 1, y, old, new);
    FloodFill_r(x, y + 1, old, new);
    FloodFill_r(x - 1, y, old, new);
    FloodFill_r(x, y - 1, old, new);
}

static bool DoLeftButtonDown(void)
{
    View * mouse_view = MouseView();
    if ( mouse_view == NULL ) return false;

    int tx, ty;
    if ( !GetMouseTile(mouse_view, &tx, &ty, _tile_size) ) {
        return false; // Mouse was in view but outside tile area.
    }

    // Start dragging a selection box?
    if ( SDL_GetModState() & SDL_KMOD_SHIFT ) {
        _fixed_x = tx;
        _fixed_y = ty;
        _state = &drag_selection_state;
        return true;
    }

    if ( mouse_view == &map->view ) {

        switch ( _tool ) {
            case TOOL_PAINT:
                if ( map->view.has_selection ) {
                    map->view.has_selection = false;
                } else {
                    if ( _show_clipboard ) {
                        ApplyClipboard();
                    } else {
                        _painting = true; // TODO: paint state
                        BeginChange(map, CHANGE_SET_TILES);
                    }
                }
                break;
            case TOOL_FILL: {
                GID old = GetMapTile(&map->map, tx, ty, _layer);
                SelectionBox * brush = CurrentBrush();
                GID new = GetTileSetGID(brush->min_x, brush->min_y);
                BeginChange(map, CHANGE_SET_TILES);
                FloodFill_r(tx, ty, old, new);
                EndChange(map);
                break;
            }

            case TOOL_LINE:
                _fixed_x = tx;
                _fixed_y = ty;
                _state = &drag_line_state;
                BeginChange(map, CHANGE_SET_TILES);
                return true;

            default:
                break;
        }
    } else if ( mouse_view == &_tileset_views[_tile_set_index] ) {
        SelectionBox * box = &_tileset_views[_tile_set_index].selection_box;
        box->min_x = box->max_x = tx;
        box->min_y = box->max_y = ty;
    }

    return true;
}

static bool DoLeftButtonUp(void)
{
    View * mouse_view = MouseView();
    if ( mouse_view == NULL ) return false;

    if ( mouse_view == &map->view && _painting ) {
        _painting = false;
        EndChange(map);
    }

    return true;
}

void SetBrushFromMap(void)
{
    GID tile = GetMapTile(&map->map, _hover_tile_x, _hover_tile_y, _layer);

    int x, y;
    GetGIDLocation(_tilesets, tile, &x, &y);

    // Get the "index" of the current tileset.
    int i = 0;
    FOR_EACH_TILESET(ts) {
        if ( ts == _active_tileset ) {
            break;
        }
        i++;
    }

    _tileset_views[i].selection_box.min_x = x;
    _tileset_views[i].selection_box.min_y = y;
    _tileset_views[i].selection_box.max_x = x;
    _tileset_views[i].selection_box.max_y = y;
}

/// Returns layer index of key in out, if valid.
bool IsLayerKey(SDL_Scancode scancode, int * out)
{
    if ( scancode < SDL_SCANCODE_1 || scancode > SDL_SCANCODE_8 ) return false;
    if ( scancode - SDL_SCANCODE_1 > map->map.num_layers ) return false;

    *out = (int)(scancode - SDL_SCANCODE_1);
    return true;
}

bool RespondToCommand(SDL_Scancode scancode)
{
    SDL_Keymod mods = SDL_GetModState();

    int l;
    if ( IsLayerKey(scancode, &l) ) {
        _layer = l; // Switch to layer
        SetStatus("Changed to Layer %d", l + 1);

        // Hide all layers except the selected one.
        for ( int i = 0; i < map->map.num_layers; i++ ) {
            _layers[i].is_visible = i == l;
        }

        return true;
    }

    switch ( scancode ) {
        case SDL_SCANCODE_C:
            if ( map->view.has_selection ) {
                CopyToClipboard();
                map->view.has_selection = false;
                _show_clipboard = true;
                SetStatus("Showing Clipboard");
                return true;
            }
            return false;

        case SDL_SCANCODE_X:
            if ( map->view.has_selection ) {
                CopyToClipboard();
                map->view.has_selection = false;
                _show_clipboard = true;
                SetStatus("Showing Clipboard");
                BeginChange(map, CHANGE_SET_TILES);
                const SelectionBox * box = &map->view.selection_box;
                for ( int y = box->min_y; y <= box->max_y; y++ ) {
                    for ( int x = box->min_x; x <= box->max_x; x++ ) {
                        GID * tile = &map->map.tiles[_layer][y * map->map.width + x];
                        GID new = 0;
                        AddTileChange(x, y, _layer, *tile, new);
                        *tile = new;
                    }
                }
                EndChange(map);
                return true;
            }
            return false;

        case SDL_SCANCODE_S:
            SaveCurrentMap();
            return true;

        case SDL_SCANCODE_Z:
            if ( mods & SDL_KMOD_SHIFT ) {
                Redo(map);
            } else {
                Undo(map);
            }
            return true;
        default:
            return false;
    }
}

void ChangeMapSize(int dx, int dy)
{
    if ( map->map.width == 1 && dx == -1 ) return;
    if ( map->map.width == MAX_MAP_WIDTH && dx == +1 ) return;
    if ( map->map.height == 1 && dy == -1 ) return;
    if ( map->map.height == MAX_MAP_HEIGHT && dy == +1 ) return;

    RegisterMapSizeChange(map, dx, dy);

    Uint16 old_w = map->map.width;
    Uint16 old_h = map->map.height;
    Uint16 new_w = (Uint16)((int)old_w + dx);
    Uint16 new_h = (Uint16)((int)old_h + dy);
    size_t size = (size_t)(new_w * new_h) * sizeof(GID);

    int source_w = new_w;
    int source_h = new_h;

    if ( new_w > map->map.width ) {
        source_w = old_w;
    }

    if ( new_h > map->map.height ) {
        source_h = old_h;
    }

    for ( int l = 0; l < map->map.num_layers; l++ ) {
        GID * new_tiles = SDL_malloc(size);

        // Copy old map tiles to new_tiles
        for ( int y = 0; y < source_h; y++ ) {
            for ( int x = 0; x < source_w; x++ ) {
                GID gid = GetMapTile(&map->map, x, y, l);
                new_tiles[y * new_w + x] = gid;
            }
        }

        // Replace pointer
        free(map->map.tiles[l]);
        map->map.tiles[l] = new_tiles;
    }

    // Update sizes
    map->map.width = new_w;
    map->map.height = new_h;

    ClampViewOrigin(&map->view); // TODO: this doesn't quite work?
}

bool RespondToKey(const SDL_KeyboardEvent * event, View * view)
{
    SDL_Keycode key = event->key;
    SDL_Scancode scancode = event->scancode;

    int l;
    if ( IsLayerKey(scancode, &l) ) {

        // Toggle layer visibility
        if ( SDL_GetModState() & SDL_KMOD_SHIFT ) {
            if ( l != _layer ) {
                _layers[l].is_visible = !_layers[l].is_visible;
                if ( _layers[l].is_visible ) {
                    SetStatus("Layer %d Shown", l + 1);
                } else {
                    SetStatus("Layer %d Hidden", l + 1);
                }
            }
        } else {
            // Change layer
            _layer = l;
            _layers[l].is_visible = true;
            SetStatus("Changed to Layer %d", l + 1);
        }
        return true;
    }

    int win_w;
    SDL_GetWindowSize(window, &win_w, NULL);

    switch ( key ) {
        case SDLK_LEFTBRACKET:
            view->next_item(-1);
            return true;

        case SDLK_RIGHTBRACKET:
            view->next_item(+1);
            return true;

        case SDLK_MINUS:
            ZoomView(view, ZOOM_OUT, NULL);
            return true;

        case SDLK_EQUALS:
            ZoomView(view, ZOOM_IN, NULL);
            return true;

        case SDLK_COMMA:
            _pal_width = SDL_max(_pal_width - PAL_WIDTH_STEP, 128);
            return true;

        case SDLK_PERIOD:
            _pal_width = SDL_min(_pal_width + PAL_WIDTH_STEP, win_w / 2);
            return true;

        case SDLK_TAB:
            if ( _tool == TOOL_PAINT ) {
                _show_clipboard = !_show_clipboard;
                if ( _show_clipboard ) {
                    SetStatus("Showing Clipboard");
                } else {
                    SetStatus("Showing Brush");
                }
                return true;
            }
            return false;

        case SDLK_E:
            _tool = TOOL_ERASE;
            return true;

        case SDLK_F:
            _tool = TOOL_FILL;
            _show_clipboard = false;
            return true;

        case SDLK_L:
            _tool = TOOL_LINE;
            _show_clipboard = false;
            return true;

        case SDLK_P:
            _tool = TOOL_PAINT;
            return true;

        case SDLK_R:
            _tool = TOOL_RECT;
            return true;

        case SDLK_F1:
            _show_grid_lines = !_show_grid_lines;
            return true;

        case SDLK_F2:
            _show_screen_lines = !_show_screen_lines;
            return true;

        case SDLK_SPACE:
            SDL_GetMouseState(&_prev_mouse.x, &_prev_mouse.y);
            _state = &drag_map_state;
            return true;

        case SDLK_LEFT:  ChangeMapSize(-1, 0); return true;
        case SDLK_RIGHT: ChangeMapSize(+1, 0); return true;
        case SDLK_UP:    ChangeMapSize(0, +1); return true;
        case SDLK_DOWN:  ChangeMapSize(0, -1); return true;

        default:
            return false;
    }
}

// TODO: this might be better with if map_view respond continue, if ... continue
static bool S_Main_Respond(const SDL_Event * event)
{
    View * mouse_view = MouseView();

    int window_width;
    SDL_GetWindowSize(window, &window_width, NULL);

    switch ( event->type ) {
        case SDL_EVENT_KEY_DOWN: {
            SDL_Keymod mods = SDL_GetModState();

            // TODO: this is dumb
            if ( mods & CTRL_KEY ) {
                return RespondToCommand(event->key.scancode);
            } else if ( mods & SDL_KMOD_SHIFT ) {
                return RespondToKey(&event->key, &_tileset_views[_tile_set_index]);
            }
            return RespondToKey(&event->key, &map->view);
        }

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            switch ( event->button.button ) {
                case SDL_BUTTON_LEFT:
                    return DoLeftButtonDown();
                case SDL_BUTTON_RIGHT:
                    return false;
                default:
                    return false;
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            switch ( event->button.button ) {
                case SDL_BUTTON_LEFT:
                    return DoLeftButtonUp();
                case SDL_BUTTON_RIGHT:
                    if ( mouse_view == &map->view ) {
                        SetBrushFromMap();
                        return true;
                    }
                    return false;
                default:
                    return false;
            }
            break;

        case SDL_EVENT_MOUSE_WHEEL: {
            if ( mouse_view ) {
                // Convert to viewport space.
                float x, y;
                SDL_GetMouseState(&x, &y);
                SDL_Point pt = {
                    .x = (int)x - mouse_view->viewport.x,
                    .y = (int)y - mouse_view->viewport.y
                };

                if ( event->wheel.y > 0 ) {
                    ZoomView(mouse_view, ZOOM_OUT, &pt);
                    return true;
                } else if ( event->wheel.y < 0 ) {
                    ZoomView(mouse_view, ZOOM_IN, &pt);
                    return true;
                }
            }
            return false;
        }

        default:
            return false;
    }
}

void S_Drag_Update(void)
{
    int tx, ty;
    if ( GetMouseTile(MouseView(), &tx, &ty, _tile_size) ) {
        _drag_x = tx;
        _drag_y = ty;
    }
}

void S_Main_Update(void)
{
    View * v = KeyRespondingView();
    ScrollView(v);

    if ( GetMouseTile(&map->view, NULL, NULL, _tile_size) ) {
        switch ( _tool ) {
            case TOOL_ERASE:
                SetCursor(CURSOR_ERASE);
                break;
            case TOOL_PAINT:
                SetCursor(CURSOR_PAINT);
                break;
            case TOOL_FILL:
                SetCursor(CURSOR_FILL);
                break;
            default:
                SetCursor(CURSOR_CROSSHAIR);
                break;
        }
    } else {
        SetCursor(CURSOR_SYSTEM);
    }

    if ( _painting ) {
        ApplyBrush();
    }
}

void RenderPaletteView(void)
{
    Tileset * ts = _active_tileset;
    int tex_w = ts->texture->w;
    int tex_h = ts->texture->h;

    View * view = &_tileset_views[_tile_set_index];
    float scale = GetScale(view->zoom_index);
    SDL_FRect dst = {
        .x = -view->origin.x * scale,
        .y = -view->origin.y * scale,
        .w = tex_w * scale,
        .h = tex_h * scale
    };

    // TODO: incorporate border and background
    RenderBorder(view->viewport);
    SDL_FRect vp_frect = RectIntToFloat(&view->viewport);
    Uint8 gray = LT_GRAY;
    SDL_SetRenderDrawColor(renderer, gray, gray, gray, 255);
    SDL_RenderFillRect(renderer, &vp_frect);

    SDL_SetRenderViewport(renderer, &view->viewport);

    RenderViewBackground(view, map->map.bg_color);
    RenderGrid(view, _bg_color, 0.2f, _tile_size, _tile_size);

    // Render the entire tileset.
    SDL_RenderTexture(renderer, _active_tileset->texture, NULL, &dst);

    SDL_SetRenderViewport(renderer, NULL);
}

void RenderHUD(void)
{
    SDL_Rect * vp = &map->view.viewport;

    int top_text_y = (int)(map->view.viewport.y - FontHeight(_font) * 1.5);
    int bottom_text_y = vp->y + vp->h + FontHeight(_font) / 2;

    // Top Map View

    if ( map->is_dirty ) {
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    }

    RenderString(_font, vp->x, top_text_y, "%s (%d*%d)",
                 map->path, map->map.width, map->map.height);

    int status_w = StringWidth(_font, "%s", _status);
    if ( status_w != 0 ) {
        int win_w;
        SDL_GetWindowSize(window, &win_w, NULL);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        int x = win_w - (status_w + UIMargin());
        RenderString(_font, x, top_text_y, "%s", _status);
    }

    // Bottom Map View

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    int x = RenderString(_font, vp->x, bottom_text_y, "%s! [", tool_names[_tool]);

    for ( int i = 0; i < map->map.num_layers; i++ ) {
        if ( i == _layer ) {
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        } else if ( !_layers[i].is_visible ) {
            SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        }

        x += RenderChar(_font, vp->x + x, bottom_text_y, '1' + i) + (int)_font->scale;
    }

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    GID gid = GetMapTile(&map->map, _hover_tile_x, _hover_tile_y, _layer);

    x += CharWidth(_font, ' ');
    char buf[16] = { 0 };
    GetZoomString(map->view.zoom_index, buf);
    RenderString(_font, vp->x + x, bottom_text_y, "%s] Tile %04x: (%d, %d) %s",
                 _layers[_layer].name,
                 gid, _hover_tile_x, _hover_tile_y, buf);

    // Palette Info

    vp = &_tileset_views[0].viewport;

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    RenderString(_font, vp->x, vp->y - (int)(FontHeight(_font) * 1.5),
                 "%s", _active_tileset->id);

    GetZoomString(_tileset_views[_tile_set_index].zoom_index, buf);
    RenderString(_font, vp->x, bottom_text_y,
                 "%s %s", buf, _show_clipboard ? "Clipboard" : "Brush");
}

void S_DragSelection_Render(void)
{
    RenderMapView();
    RenderPaletteView();
    RenderHUD();

    View * mv = MouseView();
    if ( mv ) {
        RenderSelectionBox(mv, _fixed_x, _fixed_y, _drag_x, _drag_y);
    } else {

    }
}

void LineAction_RenderTile(int x, int y, void * user)
{
    GID tile = *(GID *)user;
    SDL_FRect rect = GetTileRect(&map->view, x, y, _tile_size);
    RenderTile(renderer, tile, _tilesets, &rect);
}

void S_DragLine_Render(void)
{
    RenderMapView();
    RenderPaletteView();
    RenderHUD();

    View * mv = MouseView();
    if ( mv ) {
        SDL_Rect old_vp;
        SDL_GetRenderViewport(renderer, &old_vp);
        SDL_SetRenderViewport(renderer, &mv->viewport);
        SelectionBox * br = CurrentBrush();
        GID tile = GetTileSetGID(br->min_x, br->min_y);
        BresenhamLine(_fixed_x, _fixed_y, _drag_x, _drag_y, LineAction_RenderTile, &tile);
        SDL_SetRenderViewport(renderer, &old_vp);
    }
}

void S_Main_Render(void)
{
    View * view;
    RenderMapView();
    RenderPaletteView();
    RenderHUD();

    // Render Tileset Overlays

    view = &_tileset_views[_tile_set_index];

    SDL_SetRenderViewport(renderer, &view->viewport);

    SelectionBox * box = &view->selection_box;
    RenderSelectionBox(view, box->min_x, box->min_y, box->max_x, box->max_y);
    RenderIndicator(&_tileset_views[_tile_set_index]);

    // Render Map Overlaps

    SDL_SetRenderViewport(renderer, &map->view.viewport);

    box = &map->view.selection_box;
    if ( map->view.has_selection ) {
        RenderSelectionBox(&map->view,
                           box->min_x, box->min_y, box->max_x, box->max_y);
    }

    RenderIndicator(&map->view);

    if ( MouseView() == &map->view ) {
        if ( _show_clipboard ) {
            RenderClipboard();
        } else {
            RenderBrush();
        }
    }

    SDL_SetRenderViewport(renderer, NULL);
}

#ifdef __APPLE__
#pragma mark -
#endif

static bool _is_running = true;

static void DoEditorFrame(void)
{
    SDL_Event event;
    while ( SDL_PollEvent(&event) ) {
        if ( _state->respond && _state->respond(&event) ) {
            continue; // State processed event.
        }

        switch ( event.type ) {
            case SDL_EVENT_QUIT:
                _is_running = false;
                return;

            case SDL_EVENT_KEY_DOWN:
                switch ( event.key.key ) {
                    case SDLK_BACKSLASH:
                        ToggleFullscreen();
                        break;
                    default:
                        break;
                }
                break;

            case SDL_EVENT_WINDOW_MOVED:
            case SDL_EVENT_WINDOW_RESIZED:
                UpdateWindowFrame();
                break;

            default:
                break;
        }
    }

    _ants_phase += 20.0f * dt;

    if ( _status[0] != '\0' ) {
        _status_timer -= dt;
        if ( _status_timer <= 0.0f ) {
            _status_timer = 0.0f;
            _status[0] = '\0';
        }
    }

    // Resize things in case the window size changed.
    // TODO: only if the window size changed!
    UpdateViewSizes();

    // Update the current mouse tile.
    View * mouse_view = MouseView();
    GetMouseTile(mouse_view, &_hover_tile_x, &_hover_tile_y, _tile_size);

    if ( _state->update ) {
        _state->update();
    }

    SDL_SetRenderDrawColor(renderer, 38, 38, 38, 255);
    SDL_RenderClear(renderer);
    _state->render();

    SDL_RenderPresent(renderer);
}

int main(int argc, char ** argv)
{
    LoadArgs(argc, argv);

    // Write out a template project file if requested.
    if ( ArgIsPresent("-i") || ArgIsPresent("--init") ) {

        char * project_path = GetStrOption("--project", "-p");
        if ( project_path == NULL ) {
            project_path = DEFAULT_PROJECT_FILE;
        }

        if ( FileExists(project_path) ) {
            fprintf(stderr, "cannot init a new project: %s already exists\n",
                    project_path);
            return EXIT_FAILURE;
        }

        FILE * file = fopen(project_path, "w");
        if ( file == NULL ) {
            fprintf(stderr, "failed to create %s\n", project_path);
            return EXIT_FAILURE;
        }

        fprintf(file, "version: %d\n", PROJECT_VERSION);
        fprintf(file, "tile_size: 8\n");
        fprintf(file, "screen_size: 40 29\n");
        fprintf(file, "tile_set: \"mytiles1\"\n");
        fprintf(file, "tile_set: \"mytiles2\"\n");
        fprintf(file, "flag 0: \"Flag One\" TILE_FLAG_ONE\n");
        fprintf(file, "flag 0: \"Flag Two\" TILE_FLAG_TWO\n");
        fprintf(file, "background_color: 0x000000\n");
        fprintf(file, "default_map_size: 64 64\n");
        fprintf(file, "layer 0: \"Foreground\"\n");
        fprintf(file, "layer 1: \"Background\"\n");
        fprintf(file, "layer 2: \"Info\"\n");
        fprintf(file, "map: mymap1 32 32\n");
        fprintf(file, "map: mymap2 32 32\n");

        fclose(file);
        return EXIT_SUCCESS;
    }

    InitVideo(1280, 800, 1);
    InitSound();
    InitEditor();

    while ( _is_running ) {
        LimitFrameRate(60.0f);
        DoEditorFrame();
    }

    SaveConfig(config, EDITOR_STATE_CONFIG_FILE);
    SaveMapState();

    return 0;
}

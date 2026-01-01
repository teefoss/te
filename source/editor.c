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
 -  TODO: map size undo/redo
 -  TODO: drag map (space)
 -  TODO: tighten up start up error handling
 -  TODO: auto save on all actions, implement backup on load
 -  TODO: center map on point func: use for screen x, y change etc
 -  Tools
    * Rect
    * Replace all?
    * Others?
 -  Build in assets

    Low Priority
    ------------
 -  Backup system: save maps on launch to .backups/yymmdd-hhmmss/
 -  Flags
 -  Copy-paste Flip H and/or V

    Niceties
    --------

 -------------------------------------------------------------------------------
 BUGS
 -  View not clamped on init and some actions (resize map)
 -  Control-S not registering on first attempt? Might be just my keyboard
 -  "invalid map position"
 -  TODO: map cursor appearing when over palette
 */

#include "editor.h"

#include "args.h"
#include "asset_data.h"
#include "av.h"
#include "config.h"
#include "cursor.h"
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

#define FOCUS_OPACITY_STEP 32
#define FOCUS_OPACITY_MIN (FOCUS_OPACITY_STEP)
#define FOCUS_OPACITY_MAX (256 - (FOCUS_OPACITY_STEP))

#define FOR_EACH_TILESET(iter) \
    for (Tileset * iter = _tilesets; iter != NULL; iter = iter->next)

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

typedef struct {
    const char * name;
    SDL_Keycode key;
} ToolDef;

#define TOOL_LIST \
    X(TOOL_ERASE, "Erase", SDLK_E ) \
    X(TOOL_PAINT, "Paint", SDLK_P) \
    X(TOOL_FILL, "Fill", SDLK_F) \
    X(TOOL_LINE, "Line", SDLK_L) \
    X(TOOL_RECT, "Rect", SDLK_R) \

typedef enum {
    #define X(id, name, key) id,
    TOOL_LIST
    #undef X
    TOOL_COUNT
} Tool;

static ToolDef _tools[] = {
    #define X(id, name, key) [id] = { name, key },
    TOOL_LIST
    #undef X
};

#ifdef __APPLE__
#pragma mark - Data
#endif

// Holy global state, batman

static bool         _is_running = true;
static Font *       _font;
static int          _tile_size = 16; // Tile size in pixels.
static int          _screen_w = 0; // Visible region in tiles or 0 if unused.
static int          _screen_h = 0;
static EditorLayer  _layers[MAX_LAYERS];
static int          _num_layers;
static int          _num_tile_flags;
static Flag         _tile_flags[MAX_FLAGS];
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
static Clipboard    _clipboard;

static struct {
    bool left  : 1;
    bool right : 1;
    bool up    : 1;
    bool down  : 1;
} _keys_held;

static bool         _showing_clipboard;
static bool         _showing_screen_lines;
static bool         _showing_grid_lines = true;
static int          _unfocused_opacity = 160; // Dim unfocused screens.

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
static SDL_Color    _default_bg_color;

// Selection box
static int          _fixed_x; // Start tile of drag box
static int          _fixed_y;
static int          _drag_x; // Current drag tile
static int          _drag_y;

State * _state;

#define STATE_DEF(name) \
    static bool name##_Respond(const SDL_Event * event); \
    static void name##_Update(void); \
    static void name##_Render(void); \
    static State name = { name##_Respond, name##_Update, name##_Render };

STATE_DEF( S_DragLine )
STATE_DEF( S_DragView )
STATE_DEF( S_DragPaint )
STATE_DEF( S_DragSelection )
STATE_DEF( S_Main )

static Option config[] = {
    { CONFIG_COMMENT, "" },
    { CONFIG_COMMENT, "Editor State File - N.B. te creates and reads this file, " },
    { CONFIG_COMMENT, "do not alter it unless you know what you're doing." },
    { CONFIG_COMMENT, "" },
    { CONFIG_DEC_INT, "current_tile_set",   &_tile_set_index },
    { CONFIG_DEC_INT, "palette_zoom_0",     &_tileset_views[0].zoom_index },
    { CONFIG_DEC_INT, "palette_zoom_1",     &_tileset_views[1].zoom_index },
    { CONFIG_DEC_INT, "palette_zoom_2",     &_tileset_views[2].zoom_index },
    { CONFIG_DEC_INT, "palette_zoom_3",     &_tileset_views[3].zoom_index },
    { CONFIG_DEC_INT, "palette_zoom_4",     &_tileset_views[4].zoom_index },
    { CONFIG_DEC_INT, "palette_zoom_5",     &_tileset_views[5].zoom_index },
    { CONFIG_DEC_INT, "palette_zoom_6",     &_tileset_views[6].zoom_index },
    { CONFIG_DEC_INT, "palette_zoom_7",     &_tileset_views[7].zoom_index },
    { CONFIG_DEC_INT, "layer",              &_layer },
    { CONFIG_BOOL,    "layer_0_visible",    &_layers[0].is_visible },
    { CONFIG_BOOL,    "layer_1_visible",    &_layers[1].is_visible },
    { CONFIG_BOOL,    "layer_2_visible",    &_layers[2].is_visible },
    { CONFIG_BOOL,    "layer_3_visible",    &_layers[3].is_visible },
    { CONFIG_BOOL,    "layer_4_visible",    &_layers[4].is_visible },
    { CONFIG_BOOL,    "layer_5_visible",    &_layers[5].is_visible },
    { CONFIG_BOOL,    "layer_6_visible",    &_layers[6].is_visible },
    { CONFIG_BOOL,    "layer_7_visible",    &_layers[7].is_visible },
    { CONFIG_DEC_INT, "window_x",           &_window_frame.x },
    { CONFIG_DEC_INT, "window_y",           &_window_frame.y },
    { CONFIG_DEC_INT, "window_w",           &_window_frame.w },
    { CONFIG_DEC_INT, "window_h",           &_window_frame.h },
    { CONFIG_DEC_INT, "palette_width",      &_pal_width },
    { CONFIG_BOOL,    "show_grid_lines",    &_showing_grid_lines },
    { CONFIG_BOOL,    "show_screen_lines",  &_showing_screen_lines },
    { CONFIG_STR,     "current_map",        __current_map_name, MAP_NAME_LEN },
    { CONFIG_DEC_INT, "unfocused_opacity",  &_unfocused_opacity },

    { CONFIG_NULL },
};

#ifdef __APPLE__
#pragma mark - Prototypes
#endif

static bool A_CreateProjectFile(void);
static void A_DoEditorFrame(void);
static void A_InitEditor(void);
static void A_InitViews(void);
static void A_LoadProjectFile(void);
static void A_UpdateViewSizes(void);
static void A_UpdateWindowFrame(void);
static void E_ApplyBrush(void);
static void E_ApplyClipboard(void);
static void E_ResizeMap(int dx, int dy);
static void E_CopyToClipboard(void);
static TileRegion * E_CurrentBrush(void);
static void E_DeleteRegion(const TileRegion * region);
static void E_FloodFill_r(int x, int y, GID old, GID new);
static GID E_GetTileSetGID(int x, int y);
static void E_SetBrushFromMap(void);
static void E_SetTile(int x, int y, GID gid);
static void UI_ChangeTool(Tool new_tool);
static void UI_HideClipboard(void);
static View * UI_KeyView(void);
static int UI_Margin(void);
static View * UI_MouseView(void);
static void UI_PaletteNextItem(int direction);
static void UI_RenderBorder(SDL_Rect r);
static void UI_RenderBrush(void);
static void UI_RenderClipboard(void);
static void UI_RenderHUD(void);
static void UI_RenderIndicator(const View * view);
static void UI_RenderMapView(void);
static void UI_RenderPaletteView(void);
static void UI_RespondToGeneralEvent(const SDL_Event * event);
static void UI_SelectLayer(SDL_Keycode key);
static void UI_SetStatus(const char * fmt, ...);
static void UI_ShowClipboard(void);
static void UI_Toggle(bool * value, const char * message, const char * on, const char * off);

#ifdef __APPLE__
#pragma mark - User Interface
#endif

/// View that should respond to keyboard input.
static View * UI_KeyView(void)
{
    SDL_Keymod mods = SDL_GetModState();
    if ( mods & SDL_KMOD_SHIFT ) {
        return &_tileset_views[_tile_set_index];
    } else {
        return &__map->view;
    }
}

/// View that should respond to mouse input.
static View * UI_MouseView(void)
{
    float mx, my;
    SDL_GetMouseState(&mx, &my);
    SDL_Point mouse = { (int)mx, (int)my };

    if ( SDL_PointInRect(&mouse, &__map->view.viewport) ) {
        return &__map->view;
    } else if ( SDL_PointInRect(&mouse, &_tileset_views[0].viewport) ) {
        return &_tileset_views[_tile_set_index];
    }

    return NULL;
}

static void UI_SetStatus(const char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(_status, STATUS_LEN, fmt, args);
    va_end(args);

    _status_timer = 3.0f;
}

static void UI_ShowClipboard(void)
{
    if ( _showing_clipboard ) return; // Already showing clipboard.

    if ( _clipboard.width == 0 && _clipboard.height == 0 ) {
        UI_SetStatus("(Clipboard is Empty)");
    } else {
        _showing_clipboard = true;
        UI_SetStatus("Showing Clipboard");
    }
}

static void UI_HideClipboard(void)
{
    if ( !_showing_clipboard ) return; // Already not shown.

    UI_SetStatus("Showing Brush");
    _showing_clipboard = false;
}

static int UI_Margin(void)
{
    if ( _font == NULL ) {
        fprintf(stderr, "Error: called %s before initializing font!\n", __func__);
        exit(EXIT_FAILURE);
    }

    return FontHeight(_font);
}

static void UI_PaletteNextItem(int direction)
{
    if ( direction == -1 && _active_tileset->prev != NULL ) {
        _active_tileset = _active_tileset->prev;
        _tile_set_index--;
    } else if ( direction == 1 && _active_tileset->next != NULL ) {
        _active_tileset = _active_tileset->next;
        _tile_set_index++;
    }
}

static void UI_ChangeTool(Tool new_tool)
{
    _tool = new_tool;
    UI_SetStatus("%s!", _tools[new_tool].name);

    if ( new_tool != TOOL_PAINT ) {
        _showing_clipboard = false;
    }
}

static void UI_ChangeUnfocusedOpacity(int direction)
{
    int step = 32;
    _unfocused_opacity += direction * step;
    _unfocused_opacity = SDL_clamp(_unfocused_opacity, step, (256 - step));
    UI_SetStatus("Unfocused Screen Opacity Set to %d", _unfocused_opacity);
}

static void UI_Toggle(bool * value, const char * message, const char * on, const char * off)
{
    *value ^= 1;
    UI_SetStatus("%s %s", message, *value ? on : off);
}

static void UI_SelectLayer(SDL_Keycode key)
{
    int new_layer = (int)key - (int)SDLK_1;
    if ( new_layer > _num_layers ) return;

    SDL_Keymod mods = SDL_GetModState();

    if ( mods & SDL_KMOD_SHIFT ) {
        if ( new_layer != _layer ) {
            _layers[new_layer].is_visible = !_layers[new_layer].is_visible;
            if ( _layers[new_layer].is_visible ) {
                UI_SetStatus("Layer %d Shown", new_layer + 1);
            } else {
                UI_SetStatus("Layer %d Hidden", new_layer + 1);
            }
        }
    } else {
        _layer = new_layer; // Switch to layer
        UI_SetStatus("Changed to Layer %d", new_layer + 1);

        if ( mods & CTRL_KEY ) {
            // Hide all layers except the selected one.
            for ( int i = 0; i < __map->map.num_layers; i++ ) {
                _layers[i].is_visible = i == new_layer;
            }
        } else {
            // Unhide hidden new layer when switching to it.
            _layers[new_layer].is_visible = true;
        }
    }
}

/// Render the current hover tile with an opaque color
static void UI_RenderIndicator(const View * view)
{
    // TODO? abstract to 'render tile with background' that can be used by render
    // brush and render clipboard etc.

    SDL_Rect old_vp;
    SDL_GetRenderViewport(__renderer, &old_vp);
    SDL_SetRenderViewport(__renderer, &view->viewport);

    int tx, ty;
    if ( GetMouseTile(view, &tx, &ty, _tile_size) ) {
        SDL_FRect r = GetTileRect(view, tx, ty, _tile_size);

        if ( view == &__map->view ) {
            if ( _showing_clipboard ) {
                r.w *= _clipboard.width;
                r.h *= _clipboard.height;
            } else if ( _tool == TOOL_PAINT ) {
                // Render indicator over a possibly larger selection area
                TileRegion * br = E_CurrentBrush();
                r.w *= (br->max_x - br->min_x) + 1;
                r.h *= (br->max_y - br->min_y) + 1;
            }
        }

        // In case selection box was of zero size, the minimum is one tile.
        if ( r.w <= 0 ) r.w = _tile_size;
        if ( r.h <= 0 ) r.h = _tile_size;

        SDL_SetRenderDrawColor(__renderer, 255, 255, 255, 32);
        SDL_RenderFillRect(__renderer, &r);
    }

    SDL_SetRenderViewport(__renderer, &old_vp);
}

static void UI_RenderBrush(void)
{
    TileRegion * brush = E_CurrentBrush();

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

    float scale = GetScale(__map->view.zoom_index);
    SDL_FRect dst = {
        _hover_tile_x * _tile_size * scale,
        _hover_tile_y * _tile_size * scale,
        src.w * scale,
        src.h * scale
    };

    dst.x -= __map->view.origin.x * scale;
    dst.y -= __map->view.origin.y * scale;

    SDL_RenderTexture(__renderer, _active_tileset->texture, &src, &dst);
}

static void UI_RenderClipboard(void)
{
    View * v = UI_MouseView();
    if ( v == NULL || v != &__map->view ) {
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
                SDL_FRect dest = GetTileRect(&__map->view,
                                             dest_x,
                                             dest_y,
                                             _tile_size);
                RenderTile(__renderer, gid, _tilesets, &dest);
            }
        }
    }
}

static void UI_RenderBorder(SDL_Rect r)
{
    SDL_FRect border = {
        (float)(r.x - 1),
        (float)(r.y - 1),
        (float)(r.w + 2),
        (float)(r.h + 2)
    };
    Uint8 gray = LT_GRAY;
    SDL_SetRenderDrawColor(__renderer, gray, gray, gray, 255);
    SDL_RenderRect(__renderer, &border);
    border.x -= 1.0f;
    border.y -= 1.0f;
    border.w += 2.0f;
    border.h += 2.0f;
    SDL_RenderRect(__renderer, &border);
}

static void UI_RenderMapView(void)
{
    View * map_view = &__map->view;
    UI_RenderBorder(map_view->viewport);

    SDL_SetRenderViewport(__renderer, &map_view->viewport);

    // Background gray
    Uint8 gray = LT_GRAY;
    SDL_SetRenderDrawColor(__renderer, gray, gray, gray, 255);
    SDL_RenderFillRect(__renderer, NULL);

    RenderViewBackground(map_view, __map->map.bg_color);

    // Render Map
    // TODO: render only visible tiles

    for ( int l = 0; l < __map->map.num_layers; l++ ) {

        if ( !_layers[l].is_visible ) continue;

        for ( int y = 0; y < __map->map.height; y++ ) {
            for ( int x = 0; x < __map->map.width; x++ ) {
                GID gid = GetMapTile(&__map->map, x, y, l);
                if ( gid != 0 ) {
                    SDL_FRect dest = GetTileRect(map_view, x, y, _tile_size);
                    RenderTile(__renderer, gid, _tilesets, &dest);
                }
            }
        }
    }

    if ( _showing_grid_lines ) {
        RenderGrid(map_view,
                   __map->map.bg_color,
                   0.2f,
                   _tile_size,
                   _tile_size);
    }

    if ( _showing_screen_lines & !__map->focus_screen && _screen_w && _screen_h ) {
        RenderGrid(map_view,
                   __map->map.bg_color,
                   0.35f,
                   _tile_size * _screen_w,
                   _tile_size * _screen_h);
    }

    // Obscure unfocused screens

    if ( __map->focus_screen && _screen_w && _screen_h ) {

        int screens_per_row = __map->map.width / _screen_w;
        int screens_per_col = __map->map.height / _screen_h;

        if ( __map->map.width % _screen_w != 0 ) {
            screens_per_row++;
        }

        if ( __map->map.height % _screen_h != 0 ) {
            screens_per_col++;
        }

        for ( int y = 0; y < screens_per_col; y++ ) {
            for ( int x = 0; x < screens_per_row; x++ ) {
                if ( x != __map->screen_x || y != __map->screen_y ) {
                    SDL_FRect r = GetViewRect(map_view,
                                              x * _screen_w * _tile_size,
                                              y * _screen_h * _tile_size,
                                              _screen_w * _tile_size,
                                              _screen_h * _tile_size);

                    SetGrayAlpha(0, (Uint8)_unfocused_opacity);
                    SDL_RenderFillRect(__renderer, &r);
                }
            }
        }
    }

    SDL_SetRenderViewport(__renderer, NULL);
}

static void UI_RenderPaletteView(void)
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
    UI_RenderBorder(view->viewport);
    SDL_FRect vp_frect = RectIntToFloat(&view->viewport);
    Uint8 gray = LT_GRAY;
    SDL_SetRenderDrawColor(__renderer, gray, gray, gray, 255);
    SDL_RenderFillRect(__renderer, &vp_frect);

    SDL_SetRenderViewport(__renderer, &view->viewport);

    RenderViewBackground(view, __map->map.bg_color);
    RenderGrid(view, _bg_color, 0.2f, _tile_size, _tile_size);

    // Render the entire tileset.
    SDL_RenderTexture(__renderer, _active_tileset->texture, NULL, &dst);

    SDL_SetRenderViewport(__renderer, NULL);
}

static void UI_RenderHUD(void)
{
    SDL_Rect * vp = &__map->view.viewport;

    int top_text_y = (int)(__map->view.viewport.y - FontHeight(_font) * 1.5);
    int bottom_text_y = vp->y + vp->h + FontHeight(_font) / 2;

    // Top Map View

    if ( __map->is_dirty ) {
        SDL_SetRenderDrawColor(__renderer, 255, 0, 0, 255);
    } else {
        SDL_SetRenderDrawColor(__renderer, 255, 255, 255, 255);
    }

    RenderString(_font, vp->x, top_text_y, "%s (%d*%d)",
                 __map->path, __map->map.width, __map->map.height);

    int status_w = StringWidth(_font, "%s", _status);
    if ( status_w != 0 ) {
        int win_w;
        SDL_GetWindowSize(__window, &win_w, NULL);
        SDL_SetRenderDrawColor(__renderer, 255, 255, 255, 255);
        int x = win_w - (status_w + UI_Margin());
        RenderString(_font, x, top_text_y, "%s", _status);
    }

    // Bottom Map View

    SDL_SetRenderDrawColor(__renderer, 255, 255, 255, 255);

    int x = RenderString(_font, vp->x, bottom_text_y, "%s [", _tools[_tool].name);

    for ( int i = 0; i < __map->map.num_layers; i++ ) {
        if ( i == _layer ) {
            SDL_SetRenderDrawColor(__renderer, 255, 0, 0, 255);
        } else if ( !_layers[i].is_visible ) {
            SDL_SetRenderDrawColor(__renderer, 128, 128, 128, 255);
        } else {
            SDL_SetRenderDrawColor(__renderer, 255, 255, 255, 255);
        }

        x += RenderChar(_font, vp->x + x, bottom_text_y, '1' + i) + (int)_font->scale;
    }

    SDL_SetRenderDrawColor(__renderer, 255, 255, 255, 255);

    GID gid = GetMapTile(&__map->map, _hover_tile_x, _hover_tile_y, _layer);

    x += CharWidth(_font, ' ');
    char buf[16] = { 0 };
    GetZoomString(__map->view.zoom_index, buf);
    x += RenderString(_font, vp->x + x, bottom_text_y, "%s] Tile %04x: (%d, %d) %s",
                      _layers[_layer].name,
                      gid, _hover_tile_x, _hover_tile_y, buf);

    if ( _screen_w && _screen_h ) {
        x += RenderString(_font, vp->x + x, bottom_text_y, " Screen (%d, %d)",
                          __map->screen_x, __map->screen_y);
    }

    // Palette Info

    vp = &_tileset_views[0].viewport;

    SDL_SetRenderDrawColor(__renderer, 255, 255, 255, 255);
    RenderString(_font, vp->x, vp->y - (int)(FontHeight(_font) * 1.5),
                 "%s", _active_tileset->id);

    GetZoomString(_tileset_views[_tile_set_index].zoom_index, buf);
    RenderString(_font, vp->x, bottom_text_y,
                 "%s %s", buf, _showing_clipboard ? "Clipboard" : "Brush");
}


/// Respond to app-wide things that happen regardless of which tool is selected,
/// like save, zoom and scroll.
static void UI_RespondToGeneralEvent(const SDL_Event * event)
{
    View * mouse_view = UI_MouseView();
    View * key_view = UI_KeyView();

    SDL_Keymod mods = SDL_GetModState();

    switch ( event->type ) {
        case SDL_EVENT_QUIT:
            _is_running = false;
            return;

        case SDL_EVENT_KEY_DOWN:
            if ( event->key.repeat ) {
                return; // Ignore held keys
            }

            // Change tool?
            for ( Tool t = 0; t < TOOL_COUNT; t++ ) {
                if ( event->key.key == _tools[t].key ) {
                    UI_ChangeTool(t);
                    return;
                }
            }

            switch ( event->key.key ) {

                    // Layer Actions
                case SDLK_1:
                case SDLK_2:
                case SDLK_3:
                case SDLK_4:
                case SDLK_5:
                case SDLK_6:
                case SDLK_7:
                case SDLK_8:
                    UI_SelectLayer(event->key.key);
                    break;

                    // Copy to Clipboard
                case SDLK_C:
                    if ( (mods & CTRL_KEY) && __map->view.has_selection ) {
                        E_CopyToClipboard();
                    }
                    break;

                    // Cut to Clipboard
                case SDLK_X:
                    if ( (mods & CTRL_KEY) && __map->view.has_selection ) {
                        E_CopyToClipboard();
                        E_DeleteRegion(&__map->view.selection_box);
                        _tool = TOOL_PAINT;
                    }
                    break;

                case SDLK_A:
                    if ( mods & SDL_KMOD_ALT ) {
                        __map->screen_x = SDL_max(__map->screen_x - 1, 0);
                        UI_SetStatus("Focused Screen (%d, %d)\n", __map->screen_x, __map->screen_y);
                    } else {
                        _keys_held.left = true;
                    }
                    break;
                case SDLK_D:
                    if ( mods & SDL_KMOD_ALT ) {
                        __map->screen_x = SDL_min(__map->screen_x + 1, __map->map.width / _screen_w);
                        UI_SetStatus("Focused Screen (%d, %d)\n", __map->screen_x, __map->screen_y);
                    } else {
                        _keys_held.right = true;
                    }
                    break;
                case SDLK_S:
                    if ( mods & CTRL_KEY ) {
                        SaveCurrentMap();
                        UI_SetStatus("Saved '%s'\n", __map->path);
                    } else if ( mods & SDL_KMOD_ALT ) {
                        __map->screen_y = SDL_min(__map->screen_y + 1, __map->map.height / _screen_h);
                        UI_SetStatus("Focused Screen (%d, %d)\n", __map->screen_x, __map->screen_y);
                    } else {
                        _keys_held.down = true;
                    }
                    break;
                case SDLK_W:
                    if ( mods & SDL_KMOD_ALT ) {
                        __map->screen_y = SDL_max(__map->screen_y - 1, 0);
                        UI_SetStatus("Focused Screen (%d, %d)\n", __map->screen_x, __map->screen_y);
                    } else {
                        _keys_held.up = true;
                    }
                    break;

                case SDLK_Z:
                    if ( mods & CTRL_KEY ) {
                        if ( mods & SDL_KMOD_SHIFT ) {
                            Redo(__map);
                        } else {
                            Undo(__map);
                        }
                    }
                    break;

                    // Show/Hide grid lines
                case SDLK_F1:
                    UI_Toggle(&_showing_grid_lines, "Grid Lines", "Shown", "Hidden");
                    break;
                case SDLK_F2:
                    if ( !__map->focus_screen && _screen_w != 0 && _screen_h != 0 ) {
                        UI_Toggle(&_showing_screen_lines,
                                  "Screen Lines", "Shown", "Hidden");
                    }
                    break;
                case SDLK_F3:
                    if ( _screen_w != 0 && _screen_h != 0 ) {
                        UI_Toggle(&__map->focus_screen, "Focus Screen", "On", "Off");
                    }
                    break;

                    // Change view selection
                case SDLK_LEFTBRACKET:
                    key_view->next_item(-1);
                    break;
                case SDLK_RIGHTBRACKET:
                    key_view->next_item(+1);
                    break;

                case SDLK_MINUS:
                    if ( mods & SDL_KMOD_ALT ) {
                        UI_ChangeUnfocusedOpacity(-1);
                    } else {
                        ZoomView(key_view, ZOOM_OUT, NULL);
                    }
                    break;
                case SDLK_EQUALS:
                    if ( mods & SDL_KMOD_ALT ) {
                        UI_ChangeUnfocusedOpacity(+1);
                    } else {
                        ZoomView(key_view, ZOOM_IN, NULL);
                    }
                    break;

                    // Change palette size
                case SDLK_COMMA:
                    _pal_width = SDL_max(_pal_width - PAL_WIDTH_STEP, 128);
                    break;
                case SDLK_PERIOD:
                    _pal_width = SDL_min(_pal_width + PAL_WIDTH_STEP,
                                         _window_frame.w / 2);
                    break;

                    // Change map size
                case SDLK_LEFT:
                    E_ResizeMap(-1, 0);
                    break;
                case SDLK_RIGHT:
                    E_ResizeMap(+1, 0);
                    break;
                case SDLK_UP:
                    E_ResizeMap(0, +1);
                    break;
                case SDLK_DOWN:
                    E_ResizeMap(0, -1);
                    break;

                case SDLK_SPACE:
                    // Change to drag map state.
                    break;

                case SDLK_BACKSLASH:
                    ToggleFullscreen();
                    break;

                case SDLK_TAB:
                    if ( _tool == TOOL_PAINT ) {
                        if ( _showing_clipboard ) {
                            UI_HideClipboard();
                        } else {
                            UI_ShowClipboard();
                        }
                    }
                    break;

                default:
                    break;
            }
            break;

        case SDL_EVENT_KEY_UP:
            switch ( event->key.key ) {
                case SDLK_A: _keys_held.left = false; break;
                case SDLK_D: _keys_held.right = false; break;
                case SDLK_W: _keys_held.up = false; break;
                case SDLK_S: _keys_held.down = false; break;
            }
            break;

        case SDL_EVENT_WINDOW_MOVED:
        case SDL_EVENT_WINDOW_RESIZED:
            A_UpdateWindowFrame();
            break;

        case SDL_EVENT_MOUSE_WHEEL:
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
                } else if ( event->wheel.y < 0 ) {
                    ZoomView(mouse_view, ZOOM_IN, &pt);
                }
            }
            break;
        default:
            break;
    }
}

#ifdef __APPLE__
#pragma mark - Application
#endif

static bool A_CreateProjectFile(void)
{
    char * project_path = GetStrOption("--project", "-p");
    if ( project_path == NULL ) {
        project_path = DEFAULT_PROJECT_FILE;
    }

    if ( FileExists(project_path) ) {
        fprintf(stderr, "cannot init a new project: %s already exists\n",
                project_path);
        return false;
    }

    FILE * file = fopen(project_path, "w");
    if ( file == NULL ) {
        fprintf(stderr, "failed to create %s\n", project_path);
        return false;
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
    return true;
}

static void A_LoadProjectFile(void)
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
            ExpectString(_tile_flags[bit].name, sizeof(_tile_flags[0].name));
            AcceptIdent(_tile_flags[bit].ident, sizeof(_tile_flags[0].ident));
            _num_tile_flags++;
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

/// Update viewport and content sizes (e.g. after a window size change))
static void A_UpdateViewSizes(void)
{
    int ww, wh;
    SDL_GetWindowSize(__window, &ww, &wh);

    int text_margin = FontHeight(_font) * 2;

    SDL_Rect * vp = &_tileset_views[0].viewport;

    for ( int i = 0; i < _num_tilesets; i++ ) {
        vp = &_tileset_views[i].viewport;
        vp->w = _pal_width;
        vp->h = wh - text_margin * 2;
    }

    UpdateMapViews(vp, FontHeight(_font), _tile_size);
}

static void A_UpdateWindowFrame(void)
{
    SDL_GetWindowSize(__window, &_window_frame.w, &_window_frame.h);
    SDL_GetWindowPosition(__window, &_window_frame.x, &_window_frame.y);
}

static void A_InitViews(void)
{
    SDL_Rect * vp = &_tileset_views[0].viewport;
    Tileset * ts = _tilesets;

    for ( int i = 0; i < _num_tilesets; i++ ) {
        View * v = &_tileset_views[i];
        v->content_w = ts->texture->w;
        v->content_h = ts->texture->h;
        v->zoom_index = DefaultZoom();
        v->next_item = UI_PaletteNextItem;

        // Viewport
        vp = &_tileset_views[i].viewport;
        vp->x = FontHeight(_font);
        vp->y = FontHeight(_font) * 2;

        ts = ts->next;
    }

    InitMapViews();

    A_UpdateViewSizes();
}

static void A_InitEditor(void)
{
    // Set default layer names and visibility.
    for ( int i = 0; i < MAX_LAYERS; i++ ) {
        snprintf(_layers[i].name, sizeof(_layers[i].name), "L%d", i);
        _layers[i].is_visible = true;
    }

    A_LoadProjectFile();

    _font = LoadFontFromData(__04b03_bmp, __04b03_bmp_len, 8, 8);
    _font->scale = 3;

    InitCursors();
    A_InitViews();

    // Load Config or default values.

    SelectDefaultCurrentMap();

    if ( LoadConfig(config, EDITOR_STATE_CONFIG_FILE) ) {
        SDL_SetWindowPosition(__window, _window_frame.x, _window_frame.y);
        SDL_SetWindowSize(__window, _window_frame.w, _window_frame.h);

        SetCurrentMap(__current_map_name);
    } else {
        // Update window frame var with whatever the default is for now.
        A_UpdateWindowFrame();
    }

    LoadMapState();

    _state = &S_Main;

    ClampViewOrigin(&_tileset_views[_tile_set_index]);
}

static void A_DoEditorFrame(void)
{
    SDL_Event event;
    while ( SDL_PollEvent(&event) ) {
        if ( _state->respond && _state->respond(&event) ) continue;
        UI_RespondToGeneralEvent(&event);
    }

    UpdateAntsPhase();

    // Run status message timer---disappear when done.
    if ( _status[0] != '\0' ) {
        _status_timer -= __dt;
        if ( _status_timer <= 0.0f ) {
            _status_timer = 0.0f;
            _status[0] = '\0';
        }
    }

    // Resize things in case the window size changed.
    // TODO: only if the window size changed!
    A_UpdateViewSizes();

    // Update the current mouse tile.
    View * mouse_view = UI_MouseView();
    GetMouseTile(mouse_view, &_hover_tile_x, &_hover_tile_y, _tile_size);

    if ( _state->update ) {
        _state->update();
    }

    SDL_SetRenderDrawColor(__renderer, 38, 38, 38, 255);
    SDL_RenderClear(__renderer);

    UI_RenderMapView();
    UI_RenderPaletteView();
    UI_RenderHUD();

    _state->render();

    SDL_RenderPresent(__renderer);
}

#ifdef __APPLE__
#pragma mark - Editing
#endif

static TileRegion * E_CurrentBrush(void)
{
    return &_tileset_views[_tile_set_index].selection_box;
}

static void E_CopyToClipboard(void)
{
    if ( !__map->view.has_selection ) {
        return;
    }

    TileRegion * box = &__map->view.selection_box;
    _clipboard.width  = box->max_x - box->min_x + 1;
    _clipboard.height = box->max_y - box->min_y + 1;

    // Resize clipboard if needed.
    int slots_needed = _clipboard.width * _clipboard.height;
    if ( _clipboard.allocated_slots < slots_needed ) {
        size_t size = (size_t)slots_needed * sizeof(*_clipboard.tiles[0]);
        for ( int i = 0; i < __map->map.num_layers; i++ ) {
            _clipboard.tiles[i] = realloc(_clipboard.tiles[i], size);
        }
        _clipboard.allocated_slots = slots_needed;
    }

    _clipboard.num_copied_layers = 0;

    for ( int i = 0; i < __map->map.num_layers; i++ ) {
        if ( !_layers[i].is_visible ) continue;

        _clipboard.copied_layers[_clipboard.num_copied_layers++] = i;

        for ( int y = 0; y < _clipboard.height; y++ ) {
            for ( int x = 0; x < _clipboard.width; x++ ) {
                GID * dest = &_clipboard.tiles[i][y * _clipboard.width + x];
                int src_x = box->min_x + x;
                int src_y = box->min_y + y;
                *dest = GetMapTile(&__map->map, src_x, src_y, i);
            }
        }
    }

    __map->view.has_selection = false;
    UI_ShowClipboard();
}

static void E_ApplyClipboard(void)
{
    if ( !_showing_clipboard ) return;

    View * v = UI_MouseView();
    if ( v == NULL || v != &__map->view ) return;

    Clipboard * cb = &_clipboard;
    BeginChange(__map, CHANGE_SET_TILES);

    for ( int n = 0; n < cb->num_copied_layers; n++ ) {
        int l = cb->copied_layers[n];

        for ( int y = 0; y < cb->height; y++ ) {
            int dest_y = _hover_tile_y + y;
            if ( dest_y >= __map->map.height ) continue;

            for ( int x = 0; x < cb->width; x++ ) {
                int dest_x = _hover_tile_x + x;
                if ( dest_x >= __map->map.width ) continue;

                // TODO: convert to get/set tile
                GID * dst = &__map->map.tiles[l][dest_y * __map->map.width + dest_x];
                GID * src = &cb->tiles[l][y * cb->width + x];
                AddTileChange(dest_x, dest_y, _layer, *dst, *src);
                *dst = *src;
                __map->is_dirty = true;
            }
        }
    }

    EndChange(__map);
}

static GID E_GetTileSetGID(int x, int y)
{
    GID index = (GID)(y * _active_tileset->columns + x);
    return _active_tileset->first_gid + index;
}

static void E_ApplyBrush(void)
{
    TileRegion * brush = E_CurrentBrush();
    Map * m = &__map->map;

    int src_y = brush->min_y;
    int dst_y = _hover_tile_y;
    for ( ; src_y <= brush->max_y; src_y++, dst_y++ ) {
        if ( src_y >= m->height ) break;

        int src_x = brush->min_x;
        int dst_x = _hover_tile_x;
        for ( ; src_x <= brush->max_x; src_x++, dst_x++ ) {
            if ( src_x >= m->width ) break;

            // TODO: get/set funcs
            Uint16 dst_index = (Uint16)(dst_y * m->width + dst_x);
            Uint16 gid = E_GetTileSetGID(src_x, src_y);
            if ( m->tiles[_layer][dst_index] != gid ) {
                GID old = m->tiles[_layer][dst_index];
                m->tiles[_layer][dst_index] = gid;
                AddTileChange(dst_x, dst_y, _layer, old, gid);
                __map->is_dirty = true;
            }
        }
    }
}

static void E_FloodFill_r(int x, int y, GID old, GID new)
{
    if ( !IsValidPosition(&__map->map, x, y) ) {
        return;
    }

    GID tile = GetMapTile(&__map->map, x, y, _layer);
    if ( tile != old ) {
        return;
    }

    SetMapTile(&__map->map, x, y, _layer, new);
    AddTileChange(x, y, _layer, old, new);

    E_FloodFill_r(x + 1, y, old, new);
    E_FloodFill_r(x, y + 1, old, new);
    E_FloodFill_r(x - 1, y, old, new);
    E_FloodFill_r(x, y - 1, old, new);
}

static void E_SetBrushFromMap(void)
{
    GID tile = GetMapTile(&__map->map, _hover_tile_x, _hover_tile_y, _layer);

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

static void E_ResizeMap(int dx, int dy)
{
    int new_w = __map->map.width + dx;
    int new_h = __map->map.height + dy;

    if ( new_w <= 0 || new_w > MAX_MAP_WIDTH ) return;
    if ( new_h <= 0 || new_h > MAX_MAP_HEIGHT ) return;

    RegisterMapSizeChange(__map, dx, dy);
    ResizeMap(&__map->map, (Uint16)new_w, (Uint16)new_h);
    ClampViewOrigin(&__map->view); // TODO: this doesn't quite work?

    if ( dx > 0 ) {
        UI_SetStatus("Increased width by %d", dx);
    } else if ( dx < 0 ) {
        UI_SetStatus("Decreased width by %d", -dx);
    } else if ( dy > 0 ) {
        UI_SetStatus("Increased height by %d", dy);
    } else if ( dy < 0 ) {
        UI_SetStatus("Decreased height by %d", -dy);
    }
}

/// Set tile at (`x`, `y`) in current layer. Adds change to history.
static void E_SetTile(int x, int y, GID gid)
{
    GID old = GetMapTile(&__map->map, x, y, _layer);
    AddTileChange(x, y, _layer, old, gid);
    SetMapTile(&__map->map, x, y, _layer, gid);
}

/// Delete tiles in rectanglar region in current layer.
static void E_DeleteRegion(const TileRegion * region)
{
    BeginChange(__map, CHANGE_SET_TILES);

    for ( int y = region->min_y; y <= region->max_y; y++ ) {
        for ( int x = region->min_x; x <= region->max_x; x++ ) {
            E_SetTile(x, y, 0);
        }
    }

    EndChange(__map);
}

#ifdef __APPLE__
#pragma mark - Editor State
#endif

// Shared by multiple dragging-type states.
static void S_UpdateDrag(void)
{
    int tx, ty;
    if ( GetMouseTile(UI_MouseView(), &tx, &ty, _tile_size) ) {
        _drag_x = tx;
        _drag_y = ty;
    }
}

static bool S_DragSelection_Respond(const SDL_Event * event)
{
    View * v = UI_MouseView();

    int tx, ty;
    if ( !GetMouseTile(v, &tx, &ty, _tile_size) ) {
        return false; // Mouse inside view but outside tile area.
    }

    switch ( event->type ) {
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            TileRegion * sel = &v->selection_box;
            sel->min_x = _fixed_x;
            sel->min_y = _fixed_y;
            sel->max_x = _drag_x;
            sel->max_y = _drag_y;

            if ( sel->max_x < sel->min_x ) SWAP(sel->max_x, sel->min_x);
            if ( sel->max_y < sel->min_y ) SWAP(sel->max_y, sel->min_y);

            v->has_selection = true;
            _state = &S_Main;
            return true;
        }

        default:
            break;
    }

    return false;
}

static void S_DragSelection_Update(void)
{
    S_UpdateDrag();
}

static void S_DragSelection_Render(void)
{
    View * mv = UI_MouseView();
    if ( mv ) {
        RenderViewSelectionBox(mv,
                               _fixed_x, _fixed_y, _drag_x, _drag_y,
                               _tile_size);
    } else {

    }
}

static bool S_DragView_Respond(const SDL_Event * event)
{
    switch ( event->type ) {
        case SDL_EVENT_KEY_UP:
            if ( event->key.key == SDLK_SPACE ) {
                _state = &S_Main;
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

static void S_DragView_Update(void)
{
//    UpdateDragView(&__map->view);
    View * v = UI_MouseView();

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

static void S_DragView_Render(void)
{

}

static void S_DragLine_SetTile(int x, int y, void * user)
{
    GID old = GetMapTile(&__map->map, x, y, _layer);
    GID new = *(GID *)user;
    SetMapTile(&__map->map, x, y, _layer, new);
    AddTileChange(x, y, _layer, old, new);
}

static void S_DragLine_RenderTile(int x, int y, void * user)
{
    GID tile = *(GID *)user;
    SDL_FRect rect = GetTileRect(&__map->view, x, y, _tile_size);
    RenderTile(__renderer, tile, _tilesets, &rect);
}

static bool S_DragLine_Respond(const SDL_Event * event)
{
    View * v = UI_MouseView();

    int tx, ty;
    if ( !GetMouseTile(v, &tx, &ty, _tile_size) ) {
        return false; // Mouse inside view but outside tile area.
    }

    switch ( event->type ) {
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            TileRegion * br = E_CurrentBrush(); // TODO: GetBrushTile and replace other instances of this repeated code.
            GID tile = E_GetTileSetGID(br->min_x, br->min_y);
            BresenhamLine(_fixed_x, _fixed_y, _drag_x, _drag_y, S_DragLine_SetTile, &tile);
            EndChange(__map);
            _state = &S_Main;
            return true;
        }

        default:
            break;
    }

    return false;
}

static void S_DragLine_Update(void)
{
    S_UpdateDrag();
}

static void S_DragLine_Render(void)
{
    View * mv = UI_MouseView();
    if ( mv ) {
        SDL_Rect old_vp;
        SDL_GetRenderViewport(__renderer, &old_vp);
        SDL_SetRenderViewport(__renderer, &mv->viewport);
        TileRegion * br = E_CurrentBrush();
        GID tile = E_GetTileSetGID(br->min_x, br->min_y);
        BresenhamLine(_fixed_x, _fixed_y, _drag_x, _drag_y, S_DragLine_RenderTile, &tile);
        SDL_SetRenderViewport(__renderer, &old_vp);
    }
}

static bool S_Main_LeftMouseDown(void)
{
    View * mouse_view = UI_MouseView();
    if ( mouse_view == NULL ) return false;

    int tx, ty;
    if ( !GetMouseTile(mouse_view, &tx, &ty, _tile_size) ) {
        return false; // Mouse was in view but outside tile area.
    }

    // Start dragging a selection box?
    if ( SDL_GetModState() & SDL_KMOD_SHIFT ) {
        _fixed_x = tx;
        _fixed_y = ty;
        _state = &S_DragSelection;
        return true;
    }

    if ( mouse_view == &__map->view ) {

        if ( __map->view.has_selection ) {
            __map->view.has_selection = false;
        }

        switch ( _tool ) {
            case TOOL_PAINT:
                if ( _showing_clipboard ) {
                    E_ApplyClipboard();
                } else {
                    _state = &S_DragPaint;
                    BeginChange(__map, CHANGE_SET_TILES);
                }
                break;

            case TOOL_ERASE:
                BeginChange(__map, CHANGE_SET_TILES);
                _state = &S_DragPaint;
                break;

            case TOOL_FILL: {
                GID old = GetMapTile(&__map->map, tx, ty, _layer);
                TileRegion * brush = E_CurrentBrush();
                GID new = E_GetTileSetGID(brush->min_x, brush->min_y);
                BeginChange(__map, CHANGE_SET_TILES);
                E_FloodFill_r(tx, ty, old, new);
                EndChange(__map);
                break;
            }

            case TOOL_LINE:
                _fixed_x = tx;
                _fixed_y = ty;
                _state = &S_DragLine;
                BeginChange(__map, CHANGE_SET_TILES);
                return true;

            default:
                break;
        }
    } else if ( mouse_view == &_tileset_views[_tile_set_index] ) {
        TileRegion * box = &_tileset_views[_tile_set_index].selection_box;
        box->min_x = box->max_x = tx;
        box->min_y = box->max_y = ty;
    }

    return true;
}

static bool S_Main_Respond(const SDL_Event * event)
{
    View * mouse_view = UI_MouseView();

    switch ( event->type ) {
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            switch ( event->button.button ) {
                case SDL_BUTTON_LEFT:
                    if ( S_Main_LeftMouseDown() ) {
                        return true;
                    }
                    break;
                default:
                    break;
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            switch ( event->button.button ) {
                case SDL_BUTTON_RIGHT:
                    if ( mouse_view == &__map->view ) {
                        E_SetBrushFromMap();
                        return true;
                    }
                    break;
                default:
                    break;
            }
            break;

        default:
            break;
    }

    return false;
}

static void S_Main_Update(void)
{
    int dx = 0;
    int dy = 0;

    if ( _keys_held.left )  dx--;
    if ( _keys_held.right ) dx++;
    if ( _keys_held.up )    dy--;
    if ( _keys_held.down )  dy++;

    ScrollView(UI_KeyView(), dx, dy);

    if ( GetMouseTile(&__map->view, NULL, NULL, _tile_size) ) {
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

        if ( SDL_GetModState() & SDL_KMOD_SHIFT ) {
            SetCursor(CURSOR_CROSSHAIR);
        }
    } else {
        SetCursor(CURSOR_SYSTEM);
    }
}

static void S_Main_Render(void)
{
    View * view;

    // Render Tileset Overlays

    view = &_tileset_views[_tile_set_index];

    SDL_SetRenderViewport(__renderer, &view->viewport);

    TileRegion * box = &view->selection_box;
    RenderViewSelectionBox(view,
                           box->min_x, box->min_y, box->max_x, box->max_y,
                           _tile_size);
    UI_RenderIndicator(&_tileset_views[_tile_set_index]);

    // Render Map Overlaps

    SDL_SetRenderViewport(__renderer, &__map->view.viewport);

    box = &__map->view.selection_box;
    if ( __map->view.has_selection ) {
        RenderViewSelectionBox(&__map->view,
                               box->min_x, box->min_y, box->max_x, box->max_y,
                               _tile_size);
    }

    UI_RenderIndicator(&__map->view);

    if ( _showing_clipboard ) {
        UI_RenderClipboard();
    } else {
        switch ( _tool ) {
            case TOOL_ERASE:
                break;
            default:
                UI_RenderBrush();
                break;
        }
    }

    SDL_SetRenderViewport(__renderer, NULL);
}

static bool S_DragPaint_Respond(const SDL_Event * event)
{
    View * mouse_view = UI_MouseView();
    if ( mouse_view == NULL ) return false;
    if ( mouse_view != &__map->view ) return false;

    if ( event->type == SDL_EVENT_MOUSE_BUTTON_UP ) {
        EndChange(__map);
        _state = &S_Main;
        return true;
    }

    return false;
}

static void S_DragPaint_Update(void)
{
    switch ( _tool ) {

        case TOOL_PAINT:
            E_ApplyBrush();
            break;

        case TOOL_ERASE: {
//            TileRegion * brush = CurrentBrush();
            int x = _hover_tile_x;
            int y = _hover_tile_y;
            GID old = GetMapTile(&__map->map, x, y, _layer);
            GID new = 0;
            SetMapTile(&__map->map, x, y, _layer, new);
            AddTileChange(x, y, _layer, old, new);
            break;
        }
        default:
            break;
    }
}

static void S_DragPaint_Render(void)
{
    UI_RenderIndicator(&__map->view);
}

#ifdef __APPLE__
#pragma mark -
#endif

int main(int argc, char ** argv)
{
    (void)S_DragView; // TODO: temp

    LoadArgs(argc, argv);
    srand((unsigned)time(NULL));

    // Write out a template project file if requested.
    if ( ArgIsPresent("-i") || ArgIsPresent("--init") ) {
        if ( !A_CreateProjectFile() ) {
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }

    InitVideo(1280, 800, 1);
    InitSound();
    A_InitEditor();

    while ( _is_running ) {
        LimitFrameRate(60.0f);
        A_DoEditorFrame();
    }

    SaveConfig(config, EDITOR_STATE_CONFIG_FILE);
    SaveMapState();
    FreeMaps();

    return 0;
}

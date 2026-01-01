//
//  view.h
//  te
//
//  Created by Thomas Foster on 11/27/25.
//

#ifndef view_h
#define view_h

typedef enum {
    ZOOM_IN = 1,
    ZOOM_OUT = -1
} ZoomDirection;

/// A Selected tile region.
typedef struct {
    int min_x;
    int min_y;
    int max_x;
    int max_y;
} TileRegion;

// A rectangular region that can be zoomed and scrolled.
typedef struct view {
    SDL_Rect viewport; // Location of view in window.
    SDL_FPoint origin; // Location of upper left visible point.
    int content_w;
    int content_h;
    int zoom_index;
    void (* next_item)(int direction);
    TileRegion selection_box;
    bool has_selection;
} View;

void UpdateAntsPhase(void); // Selection box marching ants.
SDL_FRect GetVisibleRect(const View * v);
bool GetMouseTile(const View * view, int * x, int * y, int tile_size);
SDL_FRect GetTileRect(const View * view, int tile_x, int tile_y, int tile_size);
void ClampViewOrigin(View * v);
void ScrollView(View * view, int dx, int dy);
SDL_FRect GetViewRect(const View * view, int x, int y, int w, int h);

void RenderViewBackground(const View * view, SDL_Color bg);

/// Draw 'marching ants' box in `view`.
void RenderViewSelectionBox(const View * view,
                            int tx1, int ty1, int tx2, int ty2,
                            int tile_size);

void RenderGrid(const View * view,
                SDL_Color bg,
                float contrast_factor,
                int x_step,
                int y_step);

/// Zoom view in or out. `pt` is in viewport space.
void ZoomView(View * view, ZoomDirection dir, const SDL_Point * pt);

#endif /* view_h */

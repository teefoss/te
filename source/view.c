//
//  view.c
//  te
//
//  Created by Thomas Foster on 10/22/25.
//

#include "av.h"
#include "editor.h"
#include "view.h"
#include "zoom.h"
#include "misc.h"

static float _ants_phase;

static SDL_FRect GetTileUnionRect(const View * view,
                                  int x1, int y1, int x2, int y2,
                                  int tile_size)
{
    SDL_FRect t1 = GetTileRect(view, x1, y1, tile_size);
    SDL_FRect t2 = GetTileRect(view, x2, y2, tile_size);
    SDL_FRect rect;
    SDL_GetRectUnionFloat(&t1, &t2, &rect);

    return rect;
}

void UpdateAntsPhase(void)
{
    _ants_phase += 20.0f * __dt;
}

SDL_FRect GetViewRect(const View * view, int x, int y, int w, int h)
{
    float scale = GetScale(view->zoom_index);
    return (SDL_FRect){
        (x - view->origin.x) * scale,
        (y - view->origin.y) * scale,
        w * scale,
        h * scale
    };
}

SDL_FRect GetTileRect(const View * view, int tile_x, int tile_y, int tile_size)
{
    float scale = GetScale(view->zoom_index);
    return (SDL_FRect){
        (tile_x * tile_size - view->origin.x) * scale,
        (tile_y * tile_size - view->origin.y) * scale,
        tile_size * scale,
        tile_size * scale
    };
}

SDL_FRect GetVisibleRect(const View * v)
{
    float scale = GetScale(v->zoom_index);

    SDL_FRect rect;
    rect.x = v->origin.x;
    rect.y = v->origin.y;
    rect.w = v->viewport.w / scale;
    rect.h = v->viewport.h / scale;

    return rect;
}

bool GetMouseTile(const View * view, int * x, int * y, int tile_size)
{
    if ( view == NULL ) {
        return false;
    }

    float mxf, myf;
    SDL_GetMouseState(&mxf, &myf);

    SDL_Point m = { (int)mxf, (int)myf };
    if ( !SDL_PointInRect(&m, &view->viewport) ) {
        return false;
    }

    float scale = GetScale(view->zoom_index);
    int wx = (int)(view->origin.x + (mxf - view->viewport.x) / scale);
    int wy = (int)(view->origin.y + (myf - view->viewport.y) / scale);

    int tile_x = wx / tile_size;
    int tile_y = wy / tile_size;

    if ( tile_x < 0 || tile_y < 0 ) return false;
    if ( tile_x >= view->content_w / tile_size ) return false;
    if ( tile_y >= view->content_h / tile_size ) return false;

    if ( x ) *x = tile_x;
    if ( y ) *y = tile_y;
    return true;
}

SDL_FPoint ConvertToWindow(const View * view, int world_x, int world_y)
{
    float scale = GetScale(view->zoom_index);
    SDL_FPoint result;

    // Translate relative to camera origin
    result.x = (world_x - view->origin.x) * scale;
    result.y = (world_y - view->origin.y) * scale;

    return result;
}

SDL_FPoint ConvertToWorld(const View * view, int window_x, int window_y)
{
    float scale = GetScale(view->zoom_index);
    SDL_FPoint result;

    result.x = view->origin.x + window_x / scale;
    result.y = view->origin.y + window_y / scale;
    return result;
}

void RenderViewBackground(const View * view, SDL_Color bg)
{
    int w = view->content_w;
    int h = view->content_h;

    SDL_SetRenderDrawColor(__renderer, bg.r, bg.g, bg.b, bg.a);

    SDL_FPoint origin = ConvertToWindow(view, 0, 0);
    float scale = GetScale(view->zoom_index);
    SDL_FRect r = { origin.x, origin.y, w * scale, h * scale };
    SDL_RenderFillRect(__renderer, &r);
}

void RenderGrid(const View * view,
                SDL_Color bg,
                float contrast_factor,
                int x_step,
                int y_step)
{
    SetColor(ContrastingColor(bg, contrast_factor));

    // Horizontal Lines
    for ( int y = y_step; y < view->content_h; y += y_step ) {
        SDL_FPoint p1 = ConvertToWindow(view, 0, y);
        SDL_FPoint p2 = ConvertToWindow(view, view->content_w, y);
        SDL_RenderLine(__renderer, p1.x, p1.y, p2.x, p2.y);
    }

    // Vertical Lines
    for ( int x = x_step; x < view->content_w; x += x_step ) {
        SDL_FPoint p1 = ConvertToWindow(view, x, 0);
        SDL_FPoint p2 = ConvertToWindow(view, x, view->content_h);
        SDL_RenderLine(__renderer, p1.x, p1.y, p2.x, p2.y);
    }
}

void RenderViewSelectionBox(const View * view,
                            int tx1, int ty1, int tx2, int ty2,
                            int tile_size)
{
    SDL_Rect old_vp;
    SDL_GetRenderViewport(__renderer, &old_vp);
    SDL_SetRenderViewport(__renderer, &view->viewport);

    SDL_SetRenderDrawColor(__renderer, 255, 0, 0, 255);

    SDL_FRect br_rect = GetTileUnionRect(view, tx1, ty1, tx2, ty2, tile_size);

    float zoom = GetScale(view->zoom_index);
    float thickness = zoom;
    float dash_len = (tile_size / 3) * zoom;
    float dash_gap = dash_len;
    DrawDashedRect(br_rect.x, br_rect.y, br_rect.w, br_rect.h,
                   dash_len, dash_gap, thickness, _ants_phase);

    SDL_SetRenderViewport(__renderer, &old_vp);
}

void ClampViewOrigin(View * v)
{
    // Convert viewport size to content-space units.
    float scale = GetScale(v->zoom_index);
    int viewport_w = (int)(v->viewport.w / scale);
    int viewport_h = (int)(v->viewport.h / scale);

    int max_x = v->content_w - viewport_w;
    int max_y = v->content_h - viewport_h;

    if ( v->content_w < viewport_w ) {
        v->origin.x = -(viewport_w - v->content_w) / 2.0f;
    } else {
        if ( max_x <= 0 ) {
            v->origin.x = 0;
        } else {
            if ( v->origin.x < 0 ) {
                v->origin.x = 0;
            } else if ( v->origin.x > max_x ) {
                v->origin.x = (float)(max_x);
            }
        }
    }

    if ( v->content_h < viewport_h ) {
        v->origin.y = -(viewport_h - v->content_h) / 2.0f;
    } else {
        if ( max_y <= 0 ) {
            v->origin.y = 0;
        } else {
            if ( v->origin.y < 0 ) {
                v->origin.y = 0;
            } else if ( v->origin.y > max_y ) {
                v->origin.y = (float)(max_y);
            }
        }
    }
}

// TODO: change to use key up / key down
void ScrollView(View * view, int dx, int dy)
{
    if ( SDL_GetModState() & CTRL_KEY ) {
        return;  // Don't respond to CTLR-S, etc.
    }

    static float speed = 0.0f;

    if ( dx || dy ) {
        float scale = GetScale(view->zoom_index);
        speed += (900.0f / scale) * __dt;
        float max = (600.0f / scale) * __dt;
        if ( speed > max )
            speed = max;
    } else {
        speed = 0.0f;
        return;
    }

    view->origin.x += speed * dx;
    view->origin.y += speed * dy;
    ClampViewOrigin(view);
}

void ZoomView(View * view, ZoomDirection dir, const SDL_Point * pt)
{
    int x, y;
    if ( pt == NULL ) {
        x = view->viewport.w / 2;
        y = view->viewport.h / 2;
    } else {
        x = pt->x;
        y = pt->y;
    }

    SDL_FPoint center = ConvertToWorld(view, x, y);
    Zoom(&view->zoom_index, dir);
    float new_scale = GetScale(view->zoom_index);

    view->origin.x = center.x - x / new_scale;
    view->origin.y = center.y - y / new_scale;

    ClampViewOrigin(view);
}

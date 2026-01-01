//
//  misc.c
//  te
//
//  Created by Thomas Foster on 10/23/25.
//

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "editor.h"
#include "av.h"

int StringToInt(const char * str, const char * err_str)
{
    int n = (int)strtol(str, NULL, 10);
    if ( n == 0 && (errno == EINVAL || errno == ERANGE) ) {
        fprintf(stderr, "%s\n", err_str);
        exit(EXIT_FAILURE);
    }

    return n;
}

SDL_Rect RectFloatToInt(const SDL_FRect * r)
{
    return (SDL_Rect){ (int)r->x, (int)r->y, (int)r->w, (int)r->h };
}

SDL_FRect RectIntToFloat(const SDL_Rect * r)
{
    return (SDL_FRect){
        (float)(r->x),
        (float)(r->y),
        (float)(r->w),
        (float)(r->h)
    };
}

void ConvertAsepriteFile(const char * base, int scale)
{
    char cmd[1024] = { 0 };
    snprintf(cmd,
             sizeof(cmd),
             "/Applications/Aseprite.app/Contents/MacOS/aseprite -b "
             "assets/%s.aseprite --scale %d --save-as assets/%s.bmp",
             base, scale, base);

    int result = system(cmd);
    if ( result == -1 || result == 127 ) {
        fprintf(stderr, "%s failed\n", __func__);
    }
}

SDL_Color Color24ToSDL(int color24)
{
    SDL_Color color;
    color.r = (Uint8)((color24 & 0xFF0000) >> 16);
    color.g = (Uint8)((color24 & 0x00FF00) >> 8);
    color.b = (Uint8)((color24 & 0x0000FF));
    color.a = 255;

    return color;
}

static inline Uint8 clamp_channel(float v)
{
    if (v < 0.0f) return 0;
    if (v > 255.0f) return 255;
    return (Uint8)(v + 0.5f);
}

// Lighten: move each channel toward 255 by the given fraction (0–1)
static SDL_Color LightenColor(SDL_Color c, float amount)
{
    SDL_Color out = c;
    out.r = clamp_channel(c.r + (255 - c.r) * amount);
    out.g = clamp_channel(c.g + (255 - c.g) * amount);
    out.b = clamp_channel(c.b + (255 - c.b) * amount);
    return out;
}

// Darken: move each channel toward 0 by the given fraction (0–1)
static SDL_Color DarkenColor(SDL_Color c, float amount)
{
    SDL_Color out = c;
    out.r = clamp_channel(c.r * (1.0f - amount));
    out.g = clamp_channel(c.g * (1.0f - amount));
    out.b = clamp_channel(c.b * (1.0f - amount));
    return out;
}

SDL_Color ContrastingColor(SDL_Color base, float factor)
{
    // Perceived luminance (sRGB approximate)
    float brightness = 0.299f * base.r + 0.587f * base.g + 0.114f * base.b;

    if ( brightness < 128.0f ) {
        return LightenColor(base, factor);
    } else {
        return DarkenColor(base, factor);
    }
}

void DrawThickLine(float x1, float y1,
                   float x2, float y2,
                   float thickness)
{
    SDL_FRect rect;

    if (fabsf(y1 - y2) < 0.001f) {
        // Horizontal
        rect.x = x1;
        if ( x1 < x2 ) {
            rect.y = y1;// - thickness;// * 0.5f;
        } else {
            rect.y = y1 - thickness;
        }
        rect.w = x2 - x1;
        rect.h = thickness;
        SDL_RenderFillRect(__renderer, &rect);
    } else if (fabsf(x1 - x2) < 0.001f) {
        // Vertical
        //float x = x1;// - thickness;// * 0.5f;
        if ( y1 > y2 ) {
            rect.x = x1;
        } else {
            rect.x = x1 - thickness;
        }
        rect.y = y1;
        rect.w = thickness;
        rect.h = y2 - y1;
        SDL_RenderFillRect(__renderer, &rect);
    } else {
        // Should never happen for a rect,
    }
}

void DrawThickRect(SDL_FRect rect, int thickness)
{
    for ( int i = 0; i < thickness; i++ ) {
        SDL_RenderRect(__renderer, &rect);
        rect.x += 1.0f;
        rect.y += 1.0f;
        rect.w -= 2.0f;
        rect.h -= 2.0f;

        if ( rect.w <= 0 || rect.h <= 0 ) return;
    }
}

void DrawDashedLine(float x1, float y1,
                    float x2, float y2,
                    float dashLength,
                    float gapLength,
                    float thickness,
                    float phase)
{
    float dx = x2 - x1;
    float dy = y2 - y1;
    float len = sqrtf(dx*dx + dy*dy);

    if (len <= 0.0f)
        return;

    float ux = dx / len;
    float uy = dy / len;

    float pattern = dashLength + gapLength;

    // Normalize phase into [0, pattern)
    phase = fmodf(phase, pattern);
    if (phase < 0) phase += pattern;

    // Start offset along line
    float pos = -phase;

    while (pos < len) {
        if (pos + dashLength > 0) {
            float start = SDL_max(0, pos);
            float end   = SDL_min(len, pos + dashLength);

            float sx = x1 + ux * start;
            float sy = y1 + uy * start;
            float ex = x1 + ux * end;
            float ey = y1 + uy * end;

//            SDL_RenderLine(renderer, sx, sy, ex, ey);
            DrawThickLine(sx, sy, ex, ey, thickness);
        }
        pos += pattern;
    }
}

void DrawDashedRect(float x, float y,
                    float w, float h,
                    float dashLength,
                    float gapLength,
                    float thickness,
                    float phase)
{
    float x2 = x + w;
    float y2 = y + h;

    DrawDashedLine(x,  y,  x2, y,  dashLength, gapLength, thickness, phase); // top
    DrawDashedLine(x2, y,  x2, y2, dashLength, gapLength, thickness, phase); // right
    DrawDashedLine(x2, y2, x,  y2, dashLength, gapLength, thickness, phase); // bottom
    DrawDashedLine(x,  y2, x,  y,  dashLength, gapLength, thickness, phase); // left
}

bool FileExists(const char * path)
{
    FILE * f = fopen(path, "rb");
    if ( f == NULL ) return false;

    fclose(f);
    return true;
}

void _LogError(const char * func, const char * format, ...)
{
    va_list args;

    fprintf(stderr, "%s: ", func);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "\n");
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

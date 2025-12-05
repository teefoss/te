//
//  zoom.c
//  te
//
//  Created by Thomas Foster on 11/4/25.
//

#include <stdio.h>
#include <SDL3/SDL.h>

static int zoom_levels[] = {
    25, 50, 75, 100, 150, 200, 300, 400, 500, 600, 800
};

static int NumZoomLevels(void)
{
    return sizeof(zoom_levels) / sizeof(zoom_levels[0]);
}

void GetZoomString(int zoom_index, char * out)
{
    snprintf(out, 5, "%d%c", zoom_levels[zoom_index], '%');
}

float GetScale(int zoom_index)
{
    int n = NumZoomLevels();
    if ( zoom_index >= n ) {
        zoom_index = n - 1;
    }

    return zoom_levels[zoom_index] / 100.0f;
}

int SetZoom(int percent)
{
    for ( int i = 0; i < NumZoomLevels(); i++ ) {
        if ( zoom_levels[i] >= percent ) {
            return i;
        }
    }

    return 0;
}

int DefaultZoom(void)
{
    for ( int i = 0; i < NumZoomLevels(); i++ ) {
        if ( zoom_levels[i] == 100 ) {
            return i;
        }
    }

    return 0;
}

void Zoom(int * zoom_index, int delta)
{
    int min = 0;
    int max = NumZoomLevels() - 1;
    *zoom_index += delta;
    *zoom_index = SDL_clamp((*zoom_index), min, max);
}

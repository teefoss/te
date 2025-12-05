//
//  zoom.h
//  te
//
//  Created by Thomas Foster on 11/27/25.
//

#ifndef zoom_h
#define zoom_h

float GetScale(int zoom_index);

int SetZoom(int percent);

void GetZoomString(int zoom_index, char * out);

/// Returns the index for zoom of 100%
int DefaultZoom(void);

void Zoom(int * zoom_index, int delta);

#endif /* zoom_h */

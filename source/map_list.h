//
//  map_list.h
//  te
//
//  Created by Thomas Foster on 12/2/25.
//

#ifndef map_list_h
#define map_list_h

#include "editor.h"

extern EditorMap * map; // Map currently being edited.
extern char current_map_name[MAP_NAME_LEN]; // Don't touch this!

void InitMapViews(void);
// TODO: free maps

void LoadMapState(void);
void SaveMapState(void);

void SelectDefaultCurrentMap(void);
void SetCurrentMap(const char * path);
const char * CurrentMapPath(void);

void MapNextItem(int direction);
void OpenEditorMap(const char * path, int width, int height, int num_layers);
void SaveCurrentMap(void);
void UpdateMapViews(const SDL_Rect * palette_viewport, int font_height, int tile_size);

#endif /* map_list_h */

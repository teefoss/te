[te] - Tile Editor
by Tom Foster, November 2025

----------------------- ABOUT

[te] is an all-purpose tile editor supporting multiple layers. [te] is launched
from the command line and reads a project file for information about a game's
world, such as tile size, layers, and maps. A screen size can be specified for
games that have non-scrolling levels. The editor will display screen grid lines,
and individual screen can be focused.

----------------------- PROJECT FILE

Use --init or -i to create a project file called main.teproj. If a file with
this name already exists, it will not overwrite it. Properties are listed
one-per-line. An integer id follows the property name for some properties, then
follows a colon, and finally one or more parameters.

PARAMETER TYPES

    integer             values can be specified in decimal as well as hex if
                        leading with '0x' (e.g. 0xAB) or octal if leading with
                        '0' (e.g. 012).
    string (text)       values are enclosed in double quotes, e.g. "Eat Taco".

    identifier          A value containing only alphanumeric characters or
                        underscores and must begin with only alphabetic
                        characters or underscores.

PROPERTIES

    tile_size           Specify the game's tile size in pixels,
                        usually 8, 16, etc.

                        Format:
                            tile_size: [size]
                        Parameters:
                            size: an integer specifying the tile size in pixels.
                        Example:
                            tile_size: 16

    screen_size         For games with fixed-view 'screens', specify the
                        screen width and height in tiles. This property can be
                        omitted; the editor will not show screen lines.

                        Format:
                            screen_size: [width] [height]
                        Parameters:
                            width: an integer specifying the screen's width
                                in pixels.
                            height: an integer specifying the screen's height
                                in pixels.
                        Example:
                            screen_size: 40 29

    tile_set            Define a tileset to use. You can list multiple and the
                        order they appear in the project file will determine the
                        order they appear in the editor.

                        Format:
                            tile_set [id]: [path]
                        Parameters:
                            path: A string, the path of the bitmap for the
                                tileset.
                        Example:
                            tile_set 0: "graphics/tiles.bmp"

    flag                Define a tile flag. Tiles can be flagged from
                        within the palette in the editor. The editor will
                        generate 'tile_flags.h' containing an enum with these
                        flags and.

                        Format:
                            flag [id]: [name] [identifier]
                        Parameters:
                            name: a string, the name displayed in the editor
                            identifier: an identifier used in the resulting
                                flags enum.
                        Example:
                            flag 0: "Is Solid" TILE_FLAG_SOLID

    background_color    ...

    default_map_size    ...

    layer               ...

    map                 ...

----------------------- COMMAND LINE OPTIONS

-i, --init,             Initial a new project, creating a template project file
                        in the current directory.

-p, --project,          Specify name of project file to load. If not used, te
                        will try to load 'main.teproj' as the project file.

-a, --about,            Display About Information

-h, --help,             Display Help Information

----------------------- ABOUT

TODO:
- Project File
- Screen Lines explanation

----------------------- MAP CONTROLS

* N.B. on Windows/Linux use Control instead of Command

LMB                     Draw
RMB                     Pick Up Tile
Shift-LMB               Select Map Region
1, 2, 3...              Switch to layer
Shift-1, Shift-2...     Toggle layer visibility
Command-1, Command-2... Switch to layer and hide others
W,A,S,D                 Scroll
Space-LMB-Drag          Scroll
-                       Zoom Out
+                       Zoom In
[                       Previous Map
]                       Next Map
Tab                     Toggle Show Brush/Clipboard
F1                      Toggle Grid Lines
F2                      Toggle Screen Lines
P                       Switch to Paint Tool
F                       Switch to Fill Tool
L                       Switch to Line Tool
R                       Switch to Rect Tool
E                       Switch to Erase Tool
Command-S               Save Map
Command-C               Copy Selected Region
Command-X               Cut Selected Region
Command-Z               Undo
Shift-Command-Z         Redo

----------------------- TILESET CONTROLS

LMB                     Select Brush Tile
Shift-LMB               Select Brush Region
Shift-W,A,S,D           Scroll
Space-LMB-Drag          Scroll
Shift-- (minus)         Zoom Out
Shift-+ (plus)          Zoom In
[                       Previous Tileset
]                       Next Tileset
<                       Decrease Palette Size
>                       Increase Palette Size

----------------------- TODO

?                       Focus Screen
Alt-W,A,S,D             Change Highlighted Screen
Left Arrow              Decrease Map Width
Right Arrow             Increase Map Width
Down Arrow              Decrease Map Height
Up Arrow                Increase Map Height
G                       Toggle grid in front / behind

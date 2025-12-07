//
//  av.c
//
//  Created by Thomas Foster on 10/8/25.
//

#include "av.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define CELL_COLS 16

typedef struct sprite_node {
    int num;
    Sprite * data;
    struct sprite_node * next;
} SpriteNode;

SDL_Window * window;
SDL_Renderer * renderer;
int is_fullscreen;

void
InitVideo(int w, int h, int scale)
{
    if ( SDL_WasInit(SDL_INIT_VIDEO) != SDL_INIT_VIDEO ) {
        if ( !SDL_InitSubSystem(SDL_INIT_VIDEO) ) {
            goto error;
        }
    }

    SDL_WindowFlags flags = is_fullscreen ? SDL_WINDOW_FULLSCREEN : 0;
    flags |= SDL_WINDOW_RESIZABLE;
    window = SDL_CreateWindow("", w * scale, h * scale, flags);
    if ( window == NULL )
        goto error;

    renderer = SDL_CreateRenderer(window, NULL);
    if ( renderer == NULL )
        goto error;

    SDL_SetRenderVSync(renderer, 1);

//    SDL_SetRenderLogicalPresentation(
//        renderer, w, h, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    return;
error:
    fprintf(stderr, "%s failed: %s\n", __func__, SDL_GetError());
}

SDL_Texture *
LoadTextureFromBMP(const char * path)
{
    SDL_Texture * t = NULL;
    SDL_Surface * s = SDL_LoadBMP(path);

    if ( s != NULL ) {
        t = SDL_CreateTextureFromSurface(renderer, s);
        SDL_DestroySurface(s);
    }

    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);

    return t;
}

void
ToggleFullscreen(void)
{
    is_fullscreen = !is_fullscreen;
    SDL_SetWindowFullscreen(window, is_fullscreen);
}

void SetColor(SDL_Color c)
{
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
}

void SetGray(Uint8 value)
{
    SDL_SetRenderDrawColor(renderer, value, value, value, 255);
}

/// This functions destroys the surface.
static Font *
_LoadFontFromSurface(SDL_Surface * surface, Uint8 cell_w, Uint8 cell_h)
{
    Font * font = SDL_calloc(1, sizeof(*font));
    if ( font == NULL ) goto error;

    font->cell_w = cell_w;
    font->cell_h = cell_h;
    font->scale = 1.0f;

    // Calculate character widths
    for ( char ch = ' '; ch < '~' + 1; ch++ ) {
        int x = (ch % CELL_COLS) * cell_w;
        int y = (ch / CELL_COLS) * cell_h;

        // Scan for width delimiter
        Uint8 width = 0;
        for ( int scan_x = x; scan_x < x + cell_w; scan_x++ ) {
            Uint8 r, g, b;
            SDL_ReadSurfacePixel(surface, scan_x, y, &r, &g, &b, NULL);
            if ( r == 255 && g == 0 && b == 255 ) {
                break;
            } else {
                width++;
            }
        }

        if ( width == 0 ) {
            fprintf(stderr, "warning: character '%c' has zero width\n", ch);
        } else if ( width == cell_w ) {
            fprintf(
                    stderr, "warning: character '%c' missing width delimiter", ch);
        }

        font->ch_widths[(int)ch] = width;
    }

    Uint32 key = SDL_MapSurfaceRGB(surface, 0, 0, 0);
    SDL_SetSurfaceColorKey(surface, true, key);

    font->texture = SDL_CreateTextureFromSurface(renderer, surface);
    if ( font->texture == NULL ) goto error;

    SDL_DestroySurface(surface);
    SDL_SetTextureScaleMode(font->texture, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(font->texture, SDL_BLENDMODE_BLEND);
    return font;

error:
    fprintf(stderr, "%s failed: %s\n", __func__, SDL_GetError());
    SDL_DestroySurface(surface);
    return NULL;
}

Font *
LoadFontFromData(unsigned char * data, size_t data_size, Uint8 cell_w, Uint8 cell_h)
{
    SDL_IOStream * stream = SDL_IOFromConstMem(data, data_size);
    if ( stream == NULL ) {
        fprintf(stderr, "SDL_IOFromConstMem failed: %s\n", SDL_GetError());
        return NULL;
    }

    SDL_Surface * surface = SDL_LoadBMP_IO(stream, true);
    if ( surface == NULL ) {
        fprintf(stderr, "SDL_LoadBMP_IO failed: %s\n", SDL_GetError());
        return NULL;
    }

    return _LoadFontFromSurface(surface, cell_w, cell_h);
}

Font *
LoadFontFromBMP(const char * bmp_path, Uint8 cell_w, Uint8 cell_h)
{
    SDL_Surface * surface = SDL_LoadBMP(bmp_path);
    if ( surface == NULL ) {
        fprintf(stderr, "SDL_LoadBMP failed: %s\n", SDL_GetError());
        return NULL;
    }

    return _LoadFontFromSurface(surface, cell_w, cell_h);
}

int FontWidth(Font * font)
{
    return (int)(font->cell_w * font->scale);
}

int FontHeight(Font * font)
{
    return (int)(font->cell_h * font->scale);
}

int CharWidth(Font * font, char ch)
{
    return (int)(font->ch_widths[(int)ch] * font->scale);
}

int
RenderChar(Font * font, int x, int y, int ch)
{
    SDL_FRect src = {
        .x = (float)((ch % CELL_COLS) * font->cell_w),
        .y = (float)((ch / CELL_COLS) * font->cell_h),
        .w = (float)(font->ch_widths[ch]),
        .h = (float)(font->cell_h),
    };

    SDL_FRect dst = {
        .x = (float)(x),
        .y = (float)(y),
        .w = (float)(font->ch_widths[ch] * font->scale),
        .h = (float)(font->cell_h * font->scale),
    };

    Uint8 r, g, b;
    SDL_GetRenderDrawColor(renderer, &r, &g, &b, NULL);
    SDL_SetTextureColorMod(font->texture, r, g, b);
    SDL_RenderTexture(renderer, font->texture, &src, &dst);

    return (int)(font->ch_widths[ch] * font->scale);
}

int
RenderString(Font * font, int x, int y, const char * fmt, ...)
{
    char buf[256];

    size_t buf_size = sizeof(buf);
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, buf_size, fmt, args);
    va_end(args);

    size_t len = strlen(buf);

    if ( len >= buf_size ) {
        fprintf(stderr,
                "Warning: rendered string '%s' is too long. Limit: %zu",
                buf,
                buf_size);
    }

    const char * ch = buf;
    int x1 = x;
    while ( *ch ) {
        RenderChar(font, x1, y, *ch);
        x1 += (int)(font->ch_widths[(int)*ch] * font->scale);
        ch++;
    }

    return x1 - x;
}

int
StringWidth(Font * font, const char * fmt, ...)
{
    char buf[256];

    size_t buf_size = sizeof(buf);
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, buf_size, fmt, args);
    va_end(args);

    size_t len = strlen(buf);

    if ( len >= buf_size ) {
        fprintf(stderr,
                "Warning: rendered string '%s' is too long. Limit: %zu",
                buf,
                buf_size);
    }

    int width = 0;
    for ( char * c = buf; *c != '\0'; c++ ) {
        width += font->ch_widths[(int)*c];
    }

    return (int)(width * font->scale);
}

#ifdef __APPLE__
#pragma mark - SOUND
#endif

static const SDL_AudioSpec spec = {
    .format = SDL_AUDIO_S8,
    .freq = 44100,
    .channels = 1,
};
static SDL_AudioStream * stream;

Sint8 volume = 5;
int sound_on = true;

static double NoteNumberToFrequency(int note_num)
{
    static const int frequencies[] = { // in octave 6
        4186, // C
        4435, // C#
        4699, // D
        4978, // D#
        5274, // E
        5588, // F
        5920, // F#
        6272, // G
        6645, // G#
        7040, // A
        7459, // A#
        7902, // B
    };

    int octave = (note_num - 1) / 12;
    int note = (note_num - 1) % 12;
    int freq = frequencies[note];

    int octaves_down = 6 - octave;
    while ( octaves_down-- )
        freq /= 2;

    return (double)freq;
}

void QueueSound(unsigned frequency, unsigned milliseconds)
{
    if ( !sound_on ) return;

    float period = (float)spec.freq / (float)frequency;
    int len = (int)((float)spec.freq * ((float)milliseconds / 1000.0f));

    Sint8 * buf = (int8_t *)malloc((size_t)len);

    if ( buf == NULL ) {
        return;
    }

    for ( int i = 0; i < len; i++ ) {
        if ( frequency == 0 ) {
            buf[i] = 0;
        } else {
            buf[i] = (int)((float)i / period) % 2 ? volume : -volume;
        }
    }

    SDL_PutAudioStreamData(stream, buf, len);
    free(buf);
}

void InitSound(void)
{
    if ( SDL_WasInit(SDL_INIT_AUDIO) == 0 ) {
        int result = SDL_InitSubSystem(SDL_INIT_AUDIO);
        if ( result < 0 ) {
            fprintf(stderr,
                    "error: failed to init SDL audio subsystem: %s",
                    SDL_GetError());
        }
    }

    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                       &spec,
                                       NULL,
                                       NULL);

    SDL_ResumeAudioStreamDevice(stream);
    // TODO: shutdown sound
}

void SetVolume(Sint8 value)
{
    if ( value > 15 ) value = 15;
    if ( value < 1 ) value = 1;

    volume = value;
}

void PlayFreq(unsigned frequency, unsigned milliseconds)
{
    SDL_ClearAudioStream(stream);
    QueueSound(frequency, milliseconds);
}

void StopSound(void)
{
    SDL_ClearAudioStream(stream);
}

void Beep(void)
{
    PlayFreq(800, 200);
}

// Play

// L[1,2,4,8,16,32,64] default: 4
// O[0...6] default: 4
// T[32...255] default: 120

// [A...G]([+,#,-][1,2,4,8,16,32,64][.])
// N[0...84](.)
// P[v]

// TODO: figure out how to let this play simultaneously with sounds played
// from Sound()

static void PlayError(const char * msg, int line_position)
{
    printf("Play syntax error: %s (position %d)\n.", msg, line_position);
}

static void QueueNoteNumber(int note_num, unsigned note_ms, unsigned silence_ms)
{
    QueueSound((unsigned)NoteNumberToFrequency(note_num), note_ms);
    QueueSound(0, silence_ms);
}

#define PLAY_DEBUG 0
#define PLAY_STRING_MAX 255

void Play(const char * string, ...)
{
    if ( strlen(string) > PLAY_STRING_MAX ) {
        printf("Play error: string too long (max %d)\n", PLAY_STRING_MAX);
        return;
    }

    va_list args;
    va_start(args, string);

    char buffer[PLAY_STRING_MAX + 1] = { 0 };
    vsnprintf(buffer, PLAY_STRING_MAX, string, args);
    va_end(args);

    // default settings
    int bmp = 120;
    int oct = 4;
    int len = 4;
    //    int background = 1; TODO: ?

    // A-G
    static const int note_offsets[7] = { 9, 11, 0, 2, 4, 5, 7 };

    enum {
        mode_staccato = 6,  // 6/8
        mode_normal = 7,    // 7/8
        mode_legato = 8     // 8/8
    } mode = mode_normal;

    //    SDL_ClearQueuedAudio(device);
    SDL_ClearAudioStream(stream);

    // queue up whatever's in the string:

    const char * str = buffer;
    while ( *str != '\0') {
        char c = (char)toupper(*str++);
        switch ( c ) {
            case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
            case 'G': case 'N': case 'P':
            {
                // get note:
                int note = 0;
                switch ( c ) {
                    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
                    case 'G':
                        note = 1 + (oct) * 12 + note_offsets[c - 'A'];
                        break;
                    case 'P':
                        note = 0;
                        break;
                    case 'N': {
                        int number = (int)strtol(str, (char **)&str, 10);
                        if ( number < 0 || number > 84 )
{
                            PlayError("bad note number", (int)(str - string));
                            return;
                        }
                        if ( number > 0 )
                            note = number;
                        break;
                    }
                    default:
                        break;
                }

                // adjust note per accidental:
                if ( c >= 'A' && c <= 'G' ) {
                    if ( *str == '+' || *str == '#' ) {
                        if ( note < 84 )
                            note++;
                        str++;
                    } else if ( *str == '-' ) {
                        if ( note > 1 )
                            note--;
                        str++;
                    }
                }

                int d = len;

                // get note value:
                if ( c != 'N' ) {
                    int number = (int)strtol(str, (char **)&str, 10);
                    if ( number < 0 || number > 64 )
{
                        PlayError("bad note value", (int)(str - string));
                        return;
                    }
                    if ( number > 0 )
                        d = number;
                }

                // count dots:
                int dot_count = 0;
                while ( *str == '.' ) {
                    dot_count++;
                    str++;
                }

                // adjust duration if there are dots:
                float total_ms = (60.0f / (float)bmp) * 1000.0f * (4.0f / (float)d);
                float prolongation = total_ms / 2.0f;
                while ( dot_count-- ) {
                    total_ms += prolongation;
                    prolongation /= 2;
                }

                // calculate articulation silence:
                unsigned note_ms = (unsigned)(total_ms * ((float)mode / 8.0f));
                unsigned silence_ms = (unsigned)(total_ms * ((8.0f - (float)mode) / 8.0f));

                // and finally, queue it
                QueueNoteNumber(note, note_ms, silence_ms);
                break;
            } // A-G, N, and P

            case 'T':
                bmp = (int)strtol(str, (char **)&str, 10);
                if ( bmp == 0 )
{
                    PlayError("bad tempo", (int)(str - string));
                    return;
                }
#if PLAY_DEBUG
                printf("set tempo to %d\n", bmp);
#endif
                break;

            case 'O':
                if ( *str < '0' || *str > '6' )
{
                    PlayError("bad octave", (int)(str - string));
                    return;
                }
                oct = (int)strtol(str, (char **)&str, 10);
#if PLAY_DEBUG
                printf("set octave to %d\n", oct);
#endif
                break;

            case 'L':
                len = (int)strtol(str, (char **)&str, 10);
                if ( len < 1 || len > 64 )
{
                    PlayError("bad length", (int)(str - string));
                    return;
                }
#if PLAY_DEBUG
                printf("set length to %d\n", len);
#endif
                break;

            case '>':
                if ( oct < 6 )
                    oct++;
#if PLAY_DEBUG
                printf("increase octave\n");
#endif
                break;

            case '<':
                if ( oct > 0 )
                    oct--;
#if PLAY_DEBUG
                printf("decrease octave\n");
#endif
                break;

            case 'M': {
                char option = (char)toupper(*str++);
                switch ( option ) {
                    case 'L': mode = mode_legato; break;
                    case 'N': mode = mode_normal; break;
                    case 'S': mode = mode_staccato; break;
                        //                    case 'B': background = 1; break; // TODO: ?
                        //                    case 'F': background = 0; break;
                    default:
                        PlayError("bad music option", (int)(str - string));
                        return;
                }
                break;
            }
            default:
                break;
        }
    }
}

#undef PLAY_DEBUG

#ifdef __APPLE__
#pragma mark - SPRITES -
#endif

#define MAX_TEXTURE_ENTRIES 1024

typedef struct {
    SDL_Texture * texture;
    SDL_Surface * surface;
    char key[1024];
} TextureEntry;

static int tile_w = 16;
static int tile_h = 16;
static TextureEntry texture_dictionary[MAX_TEXTURE_ENTRIES];
static int num_texture_entries;
static SpriteNode * sprite_list;

static TextureEntry * GetTextureEntry(const char * key)
{
    for ( int i = 0; i < num_texture_entries; i++ ) {
        TextureEntry * entry = &texture_dictionary[i];

        if ( strncmp(key, entry->key, sizeof(entry->key)) == 0 ) {
            return entry;
        }
    }

    return NULL;
}

void SetTileSize(int w, int h)
{
    tile_w = w;
    tile_h = h;
}

void LoadSpriteClip(int sprite_num,
                    const char * bmp_path,
                    int x, int y, int w, int h,
                    int num_cels,
                    float cel_dur)
{
    // See if this texture is already loaded.
    TextureEntry * entry = GetTextureEntry(bmp_path);

    if ( entry == NULL ) {
        if ( num_texture_entries == MAX_TEXTURE_ENTRIES ) {
            fprintf(stderr, "Error: reached max number of textures\n");
            return;
        }

        // Load and add to dictionary.
        entry = &texture_dictionary[num_texture_entries];

        entry->surface = SDL_LoadBMP(bmp_path);
        if ( entry->surface == NULL ) goto sdl_error;

        entry->texture = SDL_CreateTextureFromSurface(renderer, entry->surface);
        if ( entry->texture == NULL ) goto sdl_error;

        strncpy(entry->key, bmp_path, sizeof(entry->key));
        entry->key[sizeof(entry->key) - 1] = '\0';

        SDL_SetTextureScaleMode(entry->texture, SDL_SCALEMODE_NEAREST);
        num_texture_entries++;
    }

    Sprite * sprite = SDL_calloc(1, sizeof(*sprite));
    if ( sprite == NULL ) goto alloc_error;

    sprite->texture = entry->texture;
    sprite->surface = entry->surface;
    sprite->num_cels = num_cels;
    sprite->cel_dur = cel_dur;
    sprite->location.x = x;
    sprite->location.y = y;
    sprite->location.w = w == 0 ? sprite->surface->w : w;
    sprite->location.h = h == 0 ? sprite->surface->h : h;

    SpriteNode * node = SDL_calloc(1, sizeof(*node));
    if ( node == NULL )  goto alloc_error;

    node->data = sprite;
    node->num = sprite_num;
    node->next = sprite_list;
    sprite_list = node;

    return;
alloc_error:
    fprintf(stderr, "calloc failed in %s: %s\n", __func__, strerror(errno));
    return;
sdl_error:
    fprintf(stderr, "%s failed: %s\n", __func__, SDL_GetError());
}

void LoadSprite(int sprite_num, const char * path, int x, int y, int w, int h)
{
    LoadSpriteClip(sprite_num, path, x, y, w, h, 1, 0.0f);
}

Sprite *
FindSprite(int num)
{
    for ( SpriteNode * node = sprite_list; node != NULL; node = node->next ) {
        if ( node->num == num ) {
            return node->data;
        }
    }

    return NULL;
}

void
SetSprite(AnimState * anim, int sprite_num)
{
#if 0
    if ( anim->sprite ) {
        printf("setting spr %s to ", anim->sprite->name);
    } else {
        printf("setting spr to ");
    }
#endif
    Sprite * sprite = FindSprite(sprite_num);
    anim->sprite_num = sprite_num;
    anim->sprite = sprite;
    anim->timer = sprite->cel_dur;
    SetAnimDirection(anim, anim->dir);
    //    printf("%s\n", anim->sprite->name);
}

void
SetAnimDirection(AnimState * anim, AnimDirection dir)
{
    anim->dir = dir;
    anim->timer = anim->sprite->cel_dur;

    if ( dir == ANIM_FORWARD ) {
        anim->cel = 0;
    } else if ( dir == ANIM_BACKWARD ) {
        anim->cel = anim->sprite->num_cels - 1;
    }
}

static bool
ClampCel(AnimState * anim, int num_cels)
{
    int initial_cel = anim->cel;

    if ( anim->cel < 0 ) {
        anim->cel = num_cels - 1;
    } else if ( anim->cel >= num_cels ) {
        anim->cel = 0;
    }

    return anim->cel != initial_cel;
}

static void
AdvanceCel(AnimState * anim, int num_cels)
{
    if ( anim->dir == ANIM_STOPPED ) {
        return;
    }

    anim->cel += anim->dir;

    if ( ClampCel(anim, num_cels) && !anim->loop ) {
        anim->dir = ANIM_STOPPED;
    }
}

void
UpdateAnimation(AnimState * anim, float time_delta)
{
    Sprite * spr = anim->sprite;

    if ( spr->cel_dur == 0.0f || spr->num_cels < 2 ) {
        return; // Non-animating sprite.
    }

    anim->timer -= time_delta;
    if ( anim->timer <= 0.0f ) {
        anim->timer = spr->cel_dur; // Reset timer
        AdvanceCel(anim, spr->num_cels);
    }
}

static void
_RenderSprite(Sprite * spr, int cel, SDL_FlipMode flip, int x, int y)
{
    if ( spr == NULL ) return;

    SDL_FRect src;
    src.x = (float)(spr->location.x + cel * spr->location.w);
    src.y = (float)(spr->location.y);
    src.w = (float)(spr->location.w);
    src.h = (float)(spr->location.h);

    SDL_FRect dst = {
        (float)(x),
        (float)(y),
        (float)(spr->location.w),
        (float)(spr->location.h)
    };
    SDL_RenderTextureRotated(renderer, spr->texture, &src, &dst, 0, NULL, flip);
}

void RenderAnimatedSprite(const AnimState * anim, int x, int y)
{
    _RenderSprite(anim->sprite, anim->cel, anim->flip, x, y);
}

void RenderSpriteF(int num, int cel, SDL_FlipMode flip, int x, int y)
{
    _RenderSprite(FindSprite(num), cel, flip, x, y);
}

void RenderSprite(int num, int cel, int x, int y)
{
    _RenderSprite(FindSprite(num), cel, SDL_FLIP_NONE, x, y);
}

#ifdef __APPLE__
#pragma mark - TIME -
#endif

static Uint64 last;
float dt;

void
InitTime(void)
{
    last = SDL_GetTicks();
}

void
LimitFrameRate(float fps)
{
    Uint64 now = SDL_GetTicks();
    Uint64 elapsed = now - last;
    Uint32 frame_ms = (Uint32)(1000.0f / fps);

    if ( elapsed < frame_ms ) {
        SDL_Delay(frame_ms - (Uint32)elapsed);
    }

    last = SDL_GetTicks();
    dt = 1.0f / fps;
}


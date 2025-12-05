//
//  av.h
//  Audio/Video
//
//  Created by Thomas Foster on 10/8/25.
//

#ifndef av_h
#define av_h

#include <SDL3/SDL.h>

typedef enum {
    ANIM_BACKWARD = -1,
    ANIM_STOPPED  =  0,
    ANIM_FORWARD  =  1,
} AnimDirection;

typedef struct {
    SDL_Texture * texture;
    SDL_Surface * surface;
    const char * name;
    SDL_Rect location;
    int num_cels;
    float cel_dur;
} Sprite;

typedef struct {
    int sprite_num;
    Sprite * sprite;
    AnimDirection dir;
    bool loop;
    float timer;
    int cel;
    SDL_FlipMode flip;
} AnimState;

typedef struct {
    SDL_Texture * texture;
    Uint8 cell_w;
    Uint8 cell_h;
    Uint8 ch_widths[256];
    float scale;
} Font;

extern SDL_Window * window;
extern SDL_Renderer * renderer;
extern int is_fullscreen;
extern float dt;
extern int sound_on; // Set to false to mute all sound.
extern Sint8 volume;

// VIDEO

void InitVideo(int w, int h, int scale);
void ToggleFullscreen(void);
void LimitFrameRate(float fps);
SDL_Texture * LoadTextureFromBMP(const char * path);
void SetColor(SDL_Color color);
void SetGray(Uint8 value);

// SPRITES

void SetTileSize(int w, int h);
void LoadSprite(int num, const char * path, int x, int y, int w, int h);
void LoadSpriteTile(int num, const char * path, int tile, int w, int h);
void LoadSpriteClip(int num, const char * path, int x, int y, int w, int h, int num_cels, float cel_dur);

Sprite * FindSprite(int num);

void SetSprite(AnimState * anim, int sprite_num);
void SetAnimDirection(AnimState * anim, AnimDirection dir);
void UpdateAnimation(AnimState * anim, float dt);

void RenderSprite(int num, int cel, int x, int y);
void RenderSpriteF(int num, int cel, SDL_FlipMode flip, int x, int y);
void RenderAnimatedSprite(const AnimState * anim, int x, int y);

// FONT

Font * LoadFontFromData(unsigned char * data, size_t data_size, Uint8 cell_w, Uint8 cell_h);
Font * LoadFontFromBMP(const char * bmp_path, Uint8 cell_w, Uint8 cell_h);
int RenderChar(Font * font, int x, int y, int ch);
int RenderString(Font * font, int x, int y, const char * fmt, ...);
int StringWidth(Font * font, const char * fmt, ...);
int FontWidth(Font * font);
int FontHeight(Font * font);
int CharWidth(Font * font, char ch);

// SOUND

void InitSound(void);

/// Range: 1-15. Default: 8.
void SetVolume(Sint8 value);

/// Play frequency 800 Hz for 0.2 seconds.
void Beep(void);

/// Play a frequency for given duration. Interrupts any currently playing
/// sound. To play several frequencies successively, use `QueueSound`.
void PlayFreq(unsigned frequency, unsigned milliseconds);

/// Queue a frequency, which will be played immediately. May be called
/// successively to queue multiple frequencies.
void QueueSound(unsigned frequency, unsigned milliseconds);

/// Play sound from a BASIC-style music string.
void Play(const char * string, ...);

/// Stop any sound that is currently playing.
void StopSound(void);


#endif /* av_h */

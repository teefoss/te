//
//  misc.h
//  te
//
//  Created by Thomas Foster on 11/27/25.
//

#ifndef misc_h
#define misc_h

#define STREQ(a, b) (strcmp(a, b) == 0)
#define LogError(format, ...) _LogError(__func__, format, ##__VA_ARGS__)

int StringToInt(const char * str, const char * err_str);
void _LogError(const char * func, const char * format, ...);
bool FileExists(const char * path);

SDL_Rect RectFloatToInt(const SDL_FRect * r);
SDL_FRect RectIntToFloat(const SDL_Rect * r);

void ConvertAsepriteFile(const char * base, int scale);

SDL_Color Color24ToSDL(int color24);
SDL_Color ContrastingColor(SDL_Color base, float factor);

void DrawDashedRect(float x, float y,
                    float w, float h,
                    float dashLength,
                    float gapLength,
                    float thickness,
                    float phase);

void DrawThickRect(SDL_FRect rect, int thickness);

void DrawDashedLine(float x1, float y1,
                    float x2, float y2,
                    float dashLength,
                    float gapLength,
                    float thickness,
                    float phase);

void BresenhamLine(int x1,
                   int y1,
                   int x2,
                   int y2,
                   void (* callback)(int x, int y, void * user),
                   void * user);

#endif /* misc_h */

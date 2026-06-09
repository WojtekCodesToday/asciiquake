#ifndef SCR_H
#define SCR_H

typedef struct {
    unsigned char r;
    unsigned char g;
    unsigned char b;
} ColorRGB;

enum scr_clrmode {
    SCR_CLRMODE_TRUECOLOR,
    SCR_CLRMODE_256,
    SCR_CLRMODE_8,
    SCR_CLRMODE_NONE
};

typedef struct {
    unsigned int w;
    unsigned int h;
    char* px;
    ColorRGB* color;
    char* old_px;
    ColorRGB* old_color;
    enum scr_clrmode clrmode;
} scr;
typedef enum {no,yes} booleans;
booleans scr_new(scr** s, unsigned int w, unsigned int h);
void scr_putpx(scr* s, unsigned int x, unsigned int y, char px, ColorRGB col);
void scr_clear(scr* s, char px, ColorRGB col);
void scr_draw(scr* s);

extern scr* screen;

#endif

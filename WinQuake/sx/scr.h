#ifndef SCR_H
#define SCR_H
typedef enum {no,yes} booleans;

typedef struct {
    unsigned char r;
    unsigned char g;
    unsigned char b;
} ColorRGB;

enum scr_clrmode {
    SCR_CLRMODE_TRUECOLOR,
    SCR_CLRMODE_256,
    SCR_CLRMODE_8,
    SCR_CLRMODE_NONE,
    SCR_CLRMODE_SIXEL
};

typedef struct {
    unsigned int w, h;
    enum scr_clrmode clrmode;
    char* px;
    ColorRGB* color;
    char* old_px;
    ColorRGB* old_color;
    booleans force_full_refresh; // Add this line inside your scr struct
} scr;
booleans scr_new(scr** s, unsigned int w, unsigned int h);
void scr_putpx(scr* s, unsigned int x, unsigned int y, char px, ColorRGB col);
void scr_clear(scr* s, char px, ColorRGB col);
void scr_draw(scr* s);

extern scr* screen;
extern char* render_buf;
#endif

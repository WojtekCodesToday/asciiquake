#include "scr.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

static char* render_buf = NULL;
static size_t buf_size = 1024 * 1024;

// Maps RGB to xterm-256 color index (6x6x6 cube + grayscale)
static int rgb_to_256(ColorRGB col) {
    if (col.r == col.g && col.g == col.b) {
        if (col.r < 8) return 16;
        if (col.r > 248) return 231;
        return 232 + (((col.r - 8) * 24) / 240);
    }
    return 16 + (36 * (col.r / 51)) + (6 * (col.g / 51)) + (col.b / 51);
}

booleans scr_new(scr** s, unsigned int w, unsigned int h) {
    *s = (scr*)calloc(1, sizeof(scr));
    (*s)->w = w; (*s)->h = h;
    (*s)->clrmode = SCR_CLRMODE_TRUECOLOR; // Default mode
    (*s)->px = malloc(w * h);
    (*s)->color = malloc(w * h * sizeof(ColorRGB));
    (*s)->old_px = malloc(w * h);
    (*s)->old_color = malloc(w * h * sizeof(ColorRGB));
    
    memset((*s)->old_px, 0, w * h);
    memset((*s)->old_color, 0, w * h * sizeof(ColorRGB));
    render_buf = malloc(buf_size);

    #ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode;
    GetConsoleMode(hOut, &mode);
    SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    COORD size = {(SHORT)w, (SHORT)h};
    SetConsoleScreenBufferSize(hOut, size);
    #endif

    printf("\x1b[?25l\x1b[2J");
    return yes;
}

void scr_draw(scr* s) {
    char *ptr = render_buf;
    int last_val = -1; // Unified state tracker

    for (unsigned int y = 0; y < s->h; y++) {
        for (unsigned int x = 0; x < s->w; x++) {
            unsigned int idx = y * s->w + x;
            
            if (s->px[idx] == s->old_px[idx] && memcmp(&s->color[idx], &s->old_color[idx], sizeof(ColorRGB)) == 0)
                continue;

            ptr += sprintf(ptr, "\x1b[%d;%dH", y + 1, x + 1);

            if (s->clrmode == SCR_CLRMODE_TRUECOLOR) {
                // Simplified state check for performance
                ptr += sprintf(ptr, "\x1b[38;2;%d;%d;%dm\x1b[48;2;%d;%d;%dm", 
                    s->color[idx].r, s->color[idx].g, s->color[idx].b,
                    s->color[idx].r, s->color[idx].g, s->color[idx].b);
            } 
            else if (s->clrmode == SCR_CLRMODE_256) {
                int c = rgb_to_256(s->color[idx]);
                if (c != last_val) {
                    ptr += sprintf(ptr, "\x1b[38;5;%dm\x1b[48;5;%dm", c, c);
                    last_val = c;
                }
            }
            else if (s->clrmode == SCR_CLRMODE_8) {
                // Maps to basic ANSI 30-37
                int c = 30 + ((s->color[idx].r > 127) ? 4 : 0) + ((s->color[idx].g > 127) ? 2 : 0) + ((s->color[idx].b > 127) ? 1 : 0);
                ptr += sprintf(ptr, "\x1b[%dm", c);
            }
            
            *ptr++ = s->px[idx];
            s->old_px[idx] = s->px[idx];
            s->old_color[idx] = s->color[idx];
        }
    }
    fwrite(render_buf, 1, ptr - render_buf, stdout);
    fflush(stdout);
}

void scr_putpx(scr* s, unsigned int x, unsigned int y, char px, ColorRGB col){
    if (x >= s->w || y >= s->h) return;
    unsigned int idx = y * s->w + x;
    s->px[idx] = px; 
    s->color[idx] = col;
}

void scr_clear(scr* s, char px, ColorRGB col){
    for (unsigned int i = 0; i < s->w * s->h; i++) {
        s->px[i] = px;
        s->color[i] = col;
    }
}
scr* screen;
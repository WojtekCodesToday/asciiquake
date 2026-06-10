#include "scr.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#ifdef _WIN32
#include <windows.h>
#endif

char* render_buf = NULL;
static size_t buf_size = 0;

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
    (*s)->clrmode = SCR_CLRMODE_TRUECOLOR;
    (*s)->px = malloc(w * h);
    (*s)->color = malloc(w * h * sizeof(ColorRGB));
    (*s)->old_px = malloc(w * h);
    (*s)->old_color = malloc(w * h * sizeof(ColorRGB));

    memset((*s)->px,        0,   w * h);
    memset((*s)->old_px,    0xFF, w * h); // force dirty on first frame
    memset((*s)->color,     0,   w * h * sizeof(ColorRGB));
    memset((*s)->old_color, 0xFF, w * h * sizeof(ColorRGB)); // force dirty

    buf_size = (w * h * 64) + 4096;
    render_buf = malloc(buf_size);

#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode;
    GetConsoleMode(hOut, &mode);
    SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hOut, &csbi);

    SMALL_RECT temp_rect = {0, 0, 1, 1};
    SetConsoleWindowInfo(hOut, TRUE, &temp_rect);

    COORD size = {(SHORT)w, (SHORT)h};
    SetConsoleScreenBufferSize(hOut, size);

    SMALL_RECT window_rect = {0, 0, (SHORT)(w - 1), (SHORT)(h - 1)};
    if (!SetConsoleWindowInfo(hOut, TRUE, &window_rect)) {
        window_rect.Right  = (SHORT)(csbi.dwMaximumWindowSize.X - 1);
        window_rect.Bottom = (SHORT)(csbi.dwMaximumWindowSize.Y - 1);
        SetConsoleWindowInfo(hOut, TRUE, &window_rect);
    }
#endif

    printf("\x1b[?25l\x1b[2J");
    fflush(stdout);
    return yes;
}

static inline char* fast_itoa(char *ptr, int val) {
    if (val == 0) { *ptr++ = '0'; return ptr; }
    char temp[10];
    int i = 0;
    while (val > 0) { temp[i++] = (val % 10) + '0'; val /= 10; }
    while (i > 0)   { *ptr++ = temp[--i]; }
    return ptr;
}

static inline char* emit_sixel_rle(char* ptr, unsigned char ch, int count) {
    if (count <= 3) { while (count--) *ptr++ = ch; return ptr; }
    *ptr++ = '!';
    ptr = fast_itoa(ptr, count);
    *ptr++ = ch;
    return ptr;
}

static char* write_sixel_palette(char* ptr, scr* s) {
    (void)s;
    for (int i = 0; i < 256; i++) {
        int r, g, b;
        if (i < 216) {
            r = ((i / 36) % 6) * 20;
            g = ((i / 6)  % 6) * 20;
            b = (i % 6)        * 20;
        } else {
            int gray = ((i - 216) * 100) / 39;
            if (gray > 100) gray = 100;
            r = g = b = gray;
        }
        *ptr++ = '#';
        ptr = fast_itoa(ptr, i);
        *ptr++ = ';'; *ptr++ = '2'; *ptr++ = ';';
        ptr = fast_itoa(ptr, r); *ptr++ = ';';
        ptr = fast_itoa(ptr, g); *ptr++ = ';';
        ptr = fast_itoa(ptr, b);
    }
    return ptr;
}

static inline int rgb_to_sixel_reg(ColorRGB col) {
    if (col.r == col.g && col.g == col.b) {
        if (col.r < 8)   return 216;
        if (col.r > 248) return 255;
        return 216 + (((col.r - 8) * 39) / 240);
    }
    return (36 * (col.r / 51)) + (6 * (col.g / 51)) + (col.b / 51);
}

// ============================================================================
// Flush helper — resets SGR state on the terminal side too
// ============================================================================
typedef struct {
    int      last_256_color;
    int      last_8_color;
    booleans color_set;
    ColorRGB last_color;
    int      physical_cursor_x;
    int      physical_cursor_y;
} ansi_state_t;

static void flush_and_reset_color(char **ptr_p, char **render_buf_p,
                                   ansi_state_t *st)
{
    // Emit SGR reset before the flush boundary so the terminal
    // doesn't carry over partial color state between write() calls.
    char *ptr = *ptr_p;
    *ptr++ = '\x1b'; *ptr++ = '['; *ptr++ = '0'; *ptr++ = 'm';
    fwrite(*render_buf_p, 1, ptr - *render_buf_p, stdout);
    *ptr_p = *render_buf_p; // reset ptr to start of buffer

    // Force color re-emission after the flush
    st->color_set      = no;
    st->last_256_color = -1;
    st->last_8_color   = -1;
    // Cursor position is now unknown (terminal may have scrolled etc.)
    st->physical_cursor_x = -1;
    st->physical_cursor_y = -1;
}

void scr_draw(scr* s) {
    if (!s || !s->px || !s->color || !s->old_px || !s->old_color || !render_buf)
        return;

    char *ptr     = render_buf;
    char *end_ptr = render_buf + buf_size - 2048; // larger safety margin

    // ========================================================================
    // SIXEL RENDERING MODE
    // ========================================================================
    if (s->clrmode == SCR_CLRMODE_SIXEL) {
        if (!s->w || !s->h) return;

        booleans frame_changed = no;
        if (s->force_full_refresh) {
            frame_changed = yes;
        } else {
            for (unsigned int i = 0; i < s->w * s->h; i++) {
                if (s->px[i]      != s->old_px[i]      ||
                    s->color[i].r != s->old_color[i].r ||
                    s->color[i].g != s->old_color[i].g ||
                    s->color[i].b != s->old_color[i].b) {
                    frame_changed = yes;
                    break;
                }
            }
        }

        if (!frame_changed) { s->force_full_refresh = no; return; }

        unsigned char *indexed = malloc(s->w * s->h);
        if (!indexed) return;

        for (unsigned int i = 0; i < s->w * s->h; i++) {
            indexed[i] = (unsigned char)rgb_to_sixel_reg(s->color[i]);
            s->old_color[i] = s->color[i];
            s->old_px[i]    = s->px[i];
        }

        *ptr++ = '\x1b'; *ptr++ = '['; *ptr++ = 'H';
        *ptr++ = '\x1b'; *ptr++ = 'P'; *ptr++ = 'q';
        *ptr++ = '"';
        ptr = fast_itoa(ptr, 1); *ptr++ = ';';
        ptr = fast_itoa(ptr, 1); *ptr++ = ';';
        ptr = fast_itoa(ptr, s->w); *ptr++ = ';';
        ptr = fast_itoa(ptr, s->h);

        ptr = write_sixel_palette(ptr, s);

        unsigned char *line = malloc(s->w);
        if (!line) { free(indexed); return; }

        for (unsigned int y = 0; y < s->h; y += 6) {
            unsigned char colors_in_band[256] = {0};
            int unique_colors[256];
            int unique_count = 0;

            for (unsigned int yy = y; yy < y + 6 && yy < s->h; yy++) {
                for (unsigned int x = 0; x < s->w; x++) {
                    unsigned char col = indexed[yy * s->w + x];
                    if (!colors_in_band[col]) {
                        colors_in_band[col] = 1;
                        unique_colors[unique_count++] = col;
                    }
                }
            }

            for (int i = 0; i < unique_count; i++) {
                if (ptr >= end_ptr) {
                    fwrite(render_buf, 1, ptr - render_buf, stdout);
                    ptr = render_buf;
                }

                int color = unique_colors[i];
                *ptr++ = '#';
                ptr = fast_itoa(ptr, color);

                for (unsigned int x = 0; x < s->w; x++) {
                    unsigned char bits = 0;
                    for (int bit = 0; bit < 6; bit++) {
                        unsigned int yy = y + bit;
                        if (yy >= s->h) break;
                        if (indexed[yy * s->w + x] == color)
                            bits |= (1 << bit);
                    }
                    line[x] = 63 + bits;
                }

                unsigned char prev = line[0];
                int run = 1;
                for (unsigned int x = 1; x < s->w; x++) {
                    if (line[x] == prev) { run++; }
                    else {
                        ptr = emit_sixel_rle(ptr, prev, run);
                        prev = line[x]; run = 1;
                    }
                }
                ptr = emit_sixel_rle(ptr, prev, run);
                *ptr++ = '$';
            }

            if (y + 6 < s->h) *ptr++ = '-';
        }

        *ptr++ = '\x1b'; *ptr++ = '\\';
        fwrite(render_buf, 1, ptr - render_buf, stdout);
        fflush(stdout);

        free(line);
        free(indexed);
        s->force_full_refresh = no;
        return;
    }

    // ========================================================================
    // STANDARD ANSI TEXT MODES
    // ========================================================================
    ansi_state_t st;
    st.last_256_color     = -1;
    st.last_8_color       = -1;
    st.color_set          = no;
    st.last_color.r       = 0;
    st.last_color.g       = 0;
    st.last_color.b       = 0;
    st.physical_cursor_x  = -1;
    st.physical_cursor_y  = -1;

    if (s->force_full_refresh) {
        *ptr++ = '\x1b'; *ptr++ = '['; *ptr++ = 'H'; // home
        //*ptr++ = '\x1b'; *ptr++ = '['; *ptr++ = '2'; *ptr++ = 'J'; // clear
    }

    for (unsigned int y = 0; y < s->h; y++) {
        for (unsigned int x = 0; x < s->w; x++) {
            unsigned int idx = y * s->w + x;

            if (!s->force_full_refresh) {
                if (s->px[idx]      == s->old_px[idx]      &&
                    s->color[idx].r == s->old_color[idx].r &&
                    s->color[idx].g == s->old_color[idx].g &&
                    s->color[idx].b == s->old_color[idx].b)
                {
                    // Cell unchanged — cursor position unknown after this gap
                    st.physical_cursor_x = -1;
                    st.physical_cursor_y = -1;
                    continue;
                }
            }

            // Flush buffer if near capacity, resetting SGR state cleanly
            if (ptr >= end_ptr) {
                flush_and_reset_color(&ptr, &render_buf, &st);
            }

            // Emit cursor move when not immediately after last write
            if ((int)x != st.physical_cursor_x || (int)y != st.physical_cursor_y) {
                *ptr++ = '\x1b'; *ptr++ = '[';
                ptr = fast_itoa(ptr, y + 1); *ptr++ = ';';
                ptr = fast_itoa(ptr, x + 1); *ptr++ = 'H';
            }

            // ----------------------------------------------------------------
            // Color emission
            // ----------------------------------------------------------------
            if (s->clrmode == SCR_CLRMODE_TRUECOLOR) {
                ColorRGB c = s->color[idx];
                if (!st.color_set          ||
                    c.r != st.last_color.r ||
                    c.g != st.last_color.g ||
                    c.b != st.last_color.b ||
                    s->force_full_refresh)
                {
                    // Foreground
                    *ptr++ = '\x1b'; *ptr++ = '[';
                    *ptr++ = '3'; *ptr++ = '8'; *ptr++ = ';';
                    *ptr++ = '2'; *ptr++ = ';';
                    ptr = fast_itoa(ptr, c.r); *ptr++ = ';';
                    ptr = fast_itoa(ptr, c.g); *ptr++ = ';';
                    ptr = fast_itoa(ptr, c.b); *ptr++ = 'm';
                    // Background (same color → solid block effect)
                    *ptr++ = '\x1b'; *ptr++ = '[';
                    *ptr++ = '4'; *ptr++ = '8'; *ptr++ = ';';
                    *ptr++ = '2'; *ptr++ = ';';
                    ptr = fast_itoa(ptr, c.r); *ptr++ = ';';
                    ptr = fast_itoa(ptr, c.g); *ptr++ = ';';
                    ptr = fast_itoa(ptr, c.b); *ptr++ = 'm';

                    st.last_color = c;
                    st.color_set  = yes;
                }
            }
            else if (s->clrmode == SCR_CLRMODE_256) {
                int c = rgb_to_256(s->color[idx]);
                if (c != st.last_256_color || s->force_full_refresh) {
                    *ptr++ = '\x1b'; *ptr++ = '[';
                    *ptr++ = '3'; *ptr++ = '8'; *ptr++ = ';';
                    *ptr++ = '5'; *ptr++ = ';';
                    ptr = fast_itoa(ptr, c); *ptr++ = 'm';

                    *ptr++ = '\x1b'; *ptr++ = '[';
                    *ptr++ = '4'; *ptr++ = '8'; *ptr++ = ';';
                    *ptr++ = '5'; *ptr++ = ';';
                    ptr = fast_itoa(ptr, c); *ptr++ = 'm';

                    st.last_256_color = c;
                }
            }
            else if (s->clrmode == SCR_CLRMODE_8) {
                int c = 90 + ((s->color[idx].r > 127) ? 4 : 0)
                           + ((s->color[idx].g > 127) ? 2 : 0)
                           + ((s->color[idx].b > 127) ? 1 : 0);
                if (c != st.last_8_color || s->force_full_refresh) {
                    *ptr++ = '\x1b'; *ptr++ = '[';
                    ptr = fast_itoa(ptr, c); *ptr++ = 'm';
                    st.last_8_color = c;
                }
            }
            // SCR_CLRMODE_NONE: no color escapes, just characters

            *ptr++ = s->px[idx];

            st.physical_cursor_x = (int)x + 1;
            st.physical_cursor_y = (int)y;

            s->old_px[idx]    = s->px[idx];
            s->old_color[idx] = s->color[idx];
        }
    }

    // Final SGR reset + flush
    *ptr++ = '\x1b'; *ptr++ = '['; *ptr++ = '0'; *ptr++ = 'm';
    fwrite(render_buf, 1, ptr - render_buf, stdout);
    fflush(stdout);

    s->force_full_refresh = no;
}

void scr_putpx(scr* s, unsigned int x, unsigned int y, char px, ColorRGB col) {
    if (x >= s->w || y >= s->h) return;
    unsigned int idx = y * s->w + x;
    s->px[idx]    = px;
    s->color[idx] = col;
}

void scr_clear(scr* s, char px, ColorRGB col) {
    for (unsigned int i = 0; i < s->w * s->h; i++) {
        s->px[i]    = px;
        s->color[i] = col;
    }
}

scr* screen;
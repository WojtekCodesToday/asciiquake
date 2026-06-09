/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "quakedef.h"
#include "d_local.h"
#include "sx/scr.h"
#include "sx/kb.h"   

viddef_t    vid;                

#define BASEWIDTH   320
#define BASEHEIGHT  200

#define SCREEN_HW_WIDTH   160   
#define SCREEN_HW_HEIGHT  50   

byte    vid_buffer[BASEWIDTH*BASEHEIGHT];
short   zbuffer[BASEWIDTH*BASEHEIGHT];
byte    surfcache[256*1024];

unsigned short  d_8to16table[256];
unsigned    d_8to24table[256];

static ColorRGB current_palette[256];

static qboolean mouse_avail = false;
static cvar_t   _windowed_mouse = {"_windowed_mouse", "0"};
static cvar_t   m_filter = {"m_filter", "0"};

//top ten nothingburgers
void VID_HandlePause(qboolean pause) { };
void VID_UnlockBuffer() {return; };
void VID_LockBuffer() {return; };

void    VID_SetPalette (unsigned char *palette)
{
    for (int i = 0; i < 256; i++)
    {
        current_palette[i].r = palette[i * 3];
        current_palette[i].g = palette[i * 3 + 1];
        current_palette[i].b = palette[i * 3 + 2];
    }
}

void    VID_ShiftPalette (unsigned char *palette)
{
    VID_SetPalette(palette);
}
void SCR_ParseArgs(scr* s) {
    if (COM_CheckParm("-notruecolor")) s->clrmode = SCR_CLRMODE_256;
    else if (COM_CheckParm("-nocolor")) s->clrmode = SCR_CLRMODE_NONE;
    else s->clrmode = SCR_CLRMODE_TRUECOLOR;
}
void    VID_Init (unsigned char *palette)
{
    if (!scr_new(&screen, SCREEN_HW_WIDTH, SCREEN_HW_HEIGHT))
    {
        Sys_Error("Failed to initialize custom hardware screen.\n");
    }
    SCR_ParseArgs(screen);
    vid.maxwarpwidth = vid.width = vid.conwidth = BASEWIDTH;
    vid.maxwarpheight = vid.height = vid.conheight = BASEHEIGHT;
    vid.aspect = 1.0;
    vid.numpages = 1;
    vid.colormap = host_colormap;
    vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));
    vid.buffer = vid.conbuffer = vid_buffer;
    vid.rowbytes = vid.conrowbytes = BASEWIDTH;
    
    d_pzbuffer = zbuffer;
    D_InitCaches (surfcache, sizeof(surfcache));

    VID_SetPalette(palette);
}

void    VID_Shutdown (void)
{
}

static ColorRGB SanitizeColor(ColorRGB col, enum scr_clrmode mode) {
    if (mode == SCR_CLRMODE_NONE) return (ColorRGB){255, 255, 255}; // Force white text
    if (mode == SCR_CLRMODE_8) {
        return (ColorRGB){ (col.r > 127) ? 255 : 0, 
                           (col.g > 127) ? 255 : 0, 
                           (col.b > 127) ? 255 : 0 };
    }
    return col;
}

void    VID_Update (vrect_t *rects)
{

    const char ascii_palette[] = " .:-=+*#%@";
    const int ascii_palette_len = 10;

    unsigned int target_w = screen->w;
    unsigned int target_h = screen->h;

    if (target_w == 0 || target_h == 0) return;

    int block_w = BASEWIDTH / target_w;
    int block_h = BASEHEIGHT / target_h;
    if (block_w < 1) block_w = 1;
    if (block_h < 1) block_h = 1;

    for (unsigned int out_y = 0; out_y < target_h; out_y++)
    {
        for (unsigned int out_w = 0; out_w < target_w; out_w++)
        {
            int start_x = (out_w * BASEWIDTH) / target_w;
            int start_y = (out_y * BASEHEIGHT) / target_h;

            unsigned int sum_r = 0, sum_g = 0, sum_b = 0;
            unsigned int pixel_count = 0;

            for (int by = 0; by < block_h && (start_y + by) < BASEHEIGHT; by++)
            {
                for (int bx = 0; bx < block_w && (start_x + bx) < BASEWIDTH; bx++)
                {
                    byte palette_idx = vid_buffer[(start_y + by) * BASEWIDTH + (start_x + bx)];
                    ColorRGB p_col = current_palette[palette_idx];
                    
                    sum_r += p_col.r;
                    sum_g += p_col.g;
                    sum_b += p_col.b;
                    pixel_count++;
                }
            }

            if (pixel_count == 0) continue;

            ColorRGB avg_color;
            avg_color.r = sum_r / pixel_count;
            avg_color.g = sum_g / pixel_count;
            avg_color.b = sum_b / pixel_count;

            unsigned int brightness = (avg_color.r * 2126 + avg_color.g * 7152 + avg_color.b * 722) / 10000;

            if (brightness > 0) {
                brightness = ((brightness + 60) * 15) / 10; 
            }
            if (brightness > 255) brightness = 255;

            int char_idx = (brightness * (ascii_palette_len - 1)) / 255;
            char ascii_char = ascii_palette[char_idx];

            ColorRGB final_color = SanitizeColor(avg_color, screen->clrmode);
            
            // If mode is NONE, maybe you want a different set of ASCII chars too?
            if (screen->clrmode == SCR_CLRMODE_NONE) {
                // Use a simpler, non-shaded ASCII set
                ascii_char = (brightness > 128) ? '#' : '.';
            }

            scr_putpx(screen, out_w, out_y, ascii_char, final_color);
        }
    }

    scr_draw(screen);
    fflush(stdout);
}

void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height) {}
void D_EndDirectRect (int x, int y, int width, int height) {}


// ============================================================================
// MILLISECOND TIME-DECAY INPUT HANDLING
// ============================================================================

typedef struct {
    qboolean is_pressed;
    double   last_seen_time;
} term_key_state_t;

static term_key_state_t key_registry[256];

void IN_Init (void) 
{
    kb_init(); 
    memset(key_registry, 0, sizeof(key_registry));
    Cvar_RegisterVariable(&_windowed_mouse);
    Cvar_RegisterVariable(&m_filter);
    mouse_avail = false; 
}

void IN_Shutdown (void) 
{
    kb_restore();
    if(screen){
        if(screen->px)free(screen->px);
        if(screen->color)free(screen->color);
        free(screen);
    }
    mouse_avail = false;
}

void Sys_SendKeyEvents(void) 
{
    double current_time = Sys_FloatTime();
    unsigned char buf[16];
    // Read whatever is immediately available (non-blocking)
    int n = read(STDIN_FILENO, buf, sizeof(buf));
    if (n <= 0) goto decay; // Skip to decay if no input

    for (int i = 0; i < n; i++) {
        int ch = buf[i];

        // 1. Handle Escape Sequences
        if (ch == 27 && (i + 2) < n) {
            if (buf[i+1] == '[') {
                switch(buf[i+2]) {
                    case 'A': ch = K_UPARROW; break;
                    case 'B': ch = K_DOWNARROW; break;
                    case 'C': ch = K_RIGHTARROW; break;
                    case 'D': ch = K_LEFTARROW; break;
                    default: ch = 0; break;
                }
                i += 2; // Advance index to consume sequence
            }
        }
        else if (ch == 10 || ch == 13) ch = K_ENTER;
        else if (ch >= 1 && ch <= 26)  ch += 96; // Normalize CTRL

        // 2. Register with engine
        if (ch > 0 && ch < 256) {
            if (!key_registry[ch].is_pressed) {
                Key_Event(ch, true);
                key_registry[ch].is_pressed = yes;
            }
            key_registry[ch].last_seen_time = current_time;
        }
    }

decay:
    // 3. Decay Logic
    for (int i = 0; i < 256; i++) {
        if (key_registry[i].is_pressed == yes && 
           (current_time - key_registry[i].last_seen_time) > 0.06) {
            Key_Event(i, false);
            key_registry[i].is_pressed = no;
        }
    }
}

void IN_Commands (void) {}
void IN_Move (usercmd_t *cmd) {}
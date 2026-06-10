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

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define STDIN_FILENO 0
#define read _read
#else
#include <threads.h>  // Fallback to Standard C11 Threading Support on non-Windows
#endif

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

// --- UNIFIED HYBRID RENDER POOL STORAGE ---
static byte             vid_buffer_thread[BASEWIDTH*BASEHEIGHT]; 
static volatile qboolean render_thread_running = false;
static volatile qboolean render_frame_ready = false;

#ifdef _WIN32
static HANDLE           render_thread_handle = NULL;
static CRITICAL_SECTION render_lock;
static HANDLE           render_event = NULL; 
#else
static thrd_t           render_thread_id;
static mtx_t            render_mutex;
static cnd_t            render_condition;
#endif

//top ten nothingburgers
void VID_HandlePause(qboolean pause) { };
void VID_UnlockBuffer() {return; };
void VID_LockBuffer() {return; };

void    VID_SetPalette (unsigned char *palette)
{
    for (int i = 0; i < 256; i++) {
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
    // 1. Default fallback mode
    s->clrmode = SCR_CLRMODE_TRUECOLOR;

    // 2. Check explicitly defined legacy/shorthand flags
    if (COM_CheckParm("-nocolor") || COM_CheckParm("-none") || COM_CheckParm("-monochrome")) {
        s->clrmode = SCR_CLRMODE_NONE;
        return;
    }
    
    if (COM_CheckParm("-8color") || COM_CheckParm("-ansi") || COM_CheckParm("-lowcolor")) {
        s->clrmode = SCR_CLRMODE_8;
        return;
    }

    if (COM_CheckParm("-notruecolor") || COM_CheckParm("-256color") || COM_CheckParm("-highcolor")) {
        s->clrmode = SCR_CLRMODE_256;
        return;
    }

    if (COM_CheckParm("-truecolor") || COM_CheckParm("-rgb")) {
        s->clrmode = SCR_CLRMODE_TRUECOLOR;
        return;
    }

    if (COM_CheckParm("-sixel")) {
        s->clrmode = SCR_CLRMODE_SIXEL;
        return;
    }

    // 3. Handle key-value flags: e.g., "-color=X" or "-clr=X"
    const char *key_variants[] = { "-color=", "-clr=", "-mode=" };
    for (int i = 0; i < 3; i++) {
        // Look up if our variant pattern exists anywhere in the engine arguments
        for (int arg = 1; arg < com_argc; arg++) {
            if (strncmp(com_argv[arg], key_variants[i], strlen(key_variants[i])) == 0) {
                // Point to the value right after the '=' sign
                const char *val = com_argv[arg] + strlen(key_variants[i]);
                
                if (strcmp(val, "none") == 0 || strcmp(val, "0") == 0 || strcmp(val, "mono") == 0) {
                    s->clrmode = SCR_CLRMODE_NONE;
                    return;
                }
                else if (strcmp(val, "8") == 0 || strcmp(val, "ansi") == 0) {
                    s->clrmode = SCR_CLRMODE_8;
                    return;
                }
                else if (strcmp(val, "256") == 0 || strcmp(val, "ext") == 0) {
                    s->clrmode = SCR_CLRMODE_256;
                    return;
                }
                else if (strcmp(val, "true") == 0 || strcmp(val, "rgb") == 0 || strcmp(val, "24") == 0) {
                    s->clrmode = SCR_CLRMODE_TRUECOLOR;
                    return;
                }
                else if (strcmp(val, "sixel") == 0) {
                    s->clrmode = SCR_CLRMODE_SIXEL;
                    return;
                }
            }
        }
    }
}

// Change the target array within the subsampler routine
static void VID_ProcessFrameSubsampling(void)
{
    if(!screen) return;
    const char ascii_palette[] = " .:-=+*#%@";
    const int ascii_palette_len = 10;

    unsigned int target_w = screen->w;
    unsigned int target_h = screen->h;

    if (target_w == 0 || target_h == 0) return;

    double step_x = (double)BASEWIDTH / target_w;
    double step_y = (double)BASEHEIGHT / target_h;

    for (unsigned int out_y = 0; out_y < target_h; out_y++)
    {
        for (unsigned int out_w = 0; out_w < target_w; out_w++)
        {
            int start_x = (int)(out_w * step_x);
            int start_y = (int)(out_y * step_y);
            int end_x   = (int)((out_w + 1) * step_x);
            int end_y   = (int)((out_y + 1) * step_y);

            if (end_x > BASEWIDTH)  end_x = BASEWIDTH;
            if (end_y > BASEHEIGHT) end_y = BASEHEIGHT;
            if (start_x >= end_x)   end_x = start_x + 1;
            if (start_y >= end_y)   end_y = start_y + 1;

            unsigned int sum_r = 0, sum_g = 0, sum_b = 0;
            unsigned int pixel_count = 0;

            for (int sy = start_y; sy < end_y; sy++)
            {
                for (int sx = start_x; sx < end_x; sx++)
                {
                    // FIXED: Now safely reading from the isolated asynchronous copy buffer
                    byte palette_idx = vid_buffer_thread[sy * BASEWIDTH + sx];
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

            if (screen->clrmode == SCR_CLRMODE_256 || screen->clrmode == SCR_CLRMODE_8 || screen->clrmode == SCR_CLRMODE_SIXEL) {
                avg_color.r = (avg_color.r * 13) / 10 + 15;
                avg_color.g = (avg_color.g * 13) / 10 + 15;
                avg_color.b = (avg_color.b * 13) / 10 + 15;
            }
            if (avg_color.r > 255) avg_color.r = 255;
            if (avg_color.g > 255) avg_color.g = 255;
            if (avg_color.b > 255) avg_color.b = 255;

            unsigned int brightness = (avg_color.r * 2126 + avg_color.g * 7152 + avg_color.b * 722) / 10000;

            if (brightness > 0) {
                brightness = ((brightness + 60) * 15) / 10; 
            }
            if (brightness > 255) brightness = 255;

            char ascii_char = 0;
            if (screen->clrmode == SCR_CLRMODE_NONE) {
                ascii_char = (brightness > 128) ? '#' : '.';
            } else {
                int char_idx = (brightness * (ascii_palette_len - 1)) / 255;
                ascii_char = ascii_palette[char_idx];
            }

            scr_putpx(screen, out_w, out_y, ascii_char, avg_color);
        }
    }
    if(screen) {
        scr_draw(screen);
    }
}

#ifdef _WIN32
DWORD WINAPI VID_ThreadWorkerLoop(LPVOID arg)
{
    while (render_thread_running) 
    {
        while (!render_frame_ready && render_thread_running) {
            WaitForSingleObject(render_event, INFINITE);
        }
        if (!render_thread_running) break;
        
        EnterCriticalSection(&render_lock);
        memcpy(vid_buffer_thread, vid_buffer, BASEWIDTH * BASEHEIGHT);
        render_frame_ready = false; 
        ResetEvent(render_event);
        LeaveCriticalSection(&render_lock);
        
        VID_ProcessFrameSubsampling();
    }
    return 0;
}
#else
int VID_ThreadWorkerLoop(void* arg)
{
    while (render_thread_running) 
    {
        mtx_lock(&render_mutex);
        while (!render_frame_ready && render_thread_running) {
            cnd_wait(&render_condition, &render_mutex);
        }
        if (!render_thread_running) {
            mtx_unlock(&render_mutex);
            break;
        }

        memcpy(vid_buffer_thread, vid_buffer, BASEWIDTH * BASEHEIGHT);
        
        // CRITICAL FLOW CONTROL: Reset the frame ready flag under mutex lock protection
        render_frame_ready = false; 
        mtx_unlock(&render_mutex);
        
        // Draw the frame completely un-interrupted outside of lock
        VID_ProcessFrameSubsampling();
    }
    return 0;
}
#endif

void VID_Update (vrect_t *rects)
{
    // FIX: If the background thread hasn't finished drawing the previous frame,
    // skip staging this update to prevent terminal buffer choke.
    if (render_frame_ready) {
        return; 
    }

    static double last_sync_time = 0.0;
    double current_time = Sys_FloatTime();
    
    if (last_sync_time == 0.0) {
        last_sync_time = current_time;
    }
    
    if (screen && (current_time - last_sync_time >= 4.0)) {
        screen->force_full_refresh = yes;
        last_sync_time = current_time;
    }

#ifdef _WIN32
    EnterCriticalSection(&render_lock);
    render_frame_ready = true;
    SetEvent(render_event);
    LeaveCriticalSection(&render_lock);
#else
    // Safely acquire the mutex before modifying state variables
    mtx_lock(&render_mutex);
    render_frame_ready = true;
    cnd_signal(&render_condition);
    mtx_unlock(&render_mutex);
#endif
}

void    VID_Init (unsigned char *palette)
{
    unsigned int target_width = 160;
    unsigned int target_height = 50;

    if (COM_CheckParm("-scroriginalsize") || COM_CheckParm("-originalsize"))  {
        target_width = 320;
        target_height = 200;
    } else if (COM_CheckParm("-scrlarge") || COM_CheckParm("-highres")) {
        target_width = 240;
        target_height = 75;
    }

    if (!scr_new(&screen, target_width, target_height)) {
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

    // --- ARCHITECTURE DEPENDENT ALLOCATION PASS ---
    render_thread_running = true;
#ifdef _WIN32
    InitializeCriticalSection(&render_lock);
    render_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    render_thread_handle = CreateThread(NULL, 0, VID_ThreadWorkerLoop, NULL, 0, NULL);
    if (render_thread_handle == NULL) {
        Sys_Error("Failed to initialize native Win32 background worker.");
    }
#else
    mtx_init(&render_mutex, mtx_plain);
    cnd_init(&render_condition);
    if (thrd_create(&render_thread_id, VID_ThreadWorkerLoop, NULL) != thrd_success) {
        Sys_Error("Failed to initialize standard C11 worker thread execution container.");
    }
#endif
}

void    VID_Shutdown (void)
{
    if (render_thread_running) {
#ifdef _WIN32
        EnterCriticalSection(&render_lock);
        render_thread_running = false;
        render_frame_ready = true; 
        SetEvent(render_event);
        LeaveCriticalSection(&render_lock);

        WaitForSingleObject(render_thread_handle, INFINITE);
        CloseHandle(render_thread_handle);
        CloseHandle(render_event);
        DeleteCriticalSection(&render_lock);
#else
        mtx_lock(&render_mutex);
        render_thread_running = false;
        render_frame_ready = true; 
        cnd_signal(&render_condition);
        mtx_unlock(&render_mutex);

        thrd_join(render_thread_id, NULL);
        mtx_destroy(&render_mutex);
        cnd_destroy(&render_condition);
#endif
        SLEEP_MS(16);
    }
}

static ColorRGB SanitizeColor(ColorRGB col, enum scr_clrmode mode) {
    if (mode == SCR_CLRMODE_NONE) return (ColorRGB){255, 255, 255}; 
    if (mode == SCR_CLRMODE_8) {
        return (ColorRGB){ (col.r > 127) ? 255 : 0, 
                           (col.g > 127) ? 255 : 0, 
                           (col.b > 127) ? 255 : 0 };
    }
    return col;
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

void Sys_SendKeyEvents(void) {
    double current_time = Sys_FloatTime();
    unsigned char buf[256]; // Increased storage bounds for deep read calls
    int n = 0;
    
    #if defined(_WIN32)
        n = Windows_ReadInput(buf, 256);
    #else
        n = read(STDIN_FILENO, buf, sizeof(buf));
    #endif

    if (n <= 0) goto decay;

    for (int i = 0; i < n; i++) {
        int ch = buf[i];
        
        #if defined(_WIN32)
            if (ch & 0x80) {
                switch(ch & 0x7F) {
                    case 'A': ch = K_UPARROW; break;
                    case 'B': ch = K_DOWNARROW; break;
                    case 'C': ch = K_RIGHTARROW; break;
                    case 'D': ch = K_LEFTARROW; break;
                }
            }
            else if (ch == 13 || ch == 10) ch = K_ENTER;
            else if (ch == 127)            ch = K_BACKSPACE;
            else if (ch == 9)              ch = K_TAB;
            else if (ch == 27)             ch = K_ESCAPE;
            else if (ch >= 1 && ch <= 26 && ch != K_CTRL) {
                // Synthesize Ctrl state engine mappings for Windows compatibility layer
                Key_Event(K_CTRL, true);
                key_registry[K_CTRL].is_pressed = yes;
                key_registry[K_CTRL].last_seen_time = current_time;
                ch = ch + 96; 
            }
        #else
// --- FIXED LINUX MULTI-BYTE ESCAPE PARSER ---
            if (ch == 27) {
                // Check if this is a genuine multi-byte terminal escape sequence (e.g., Arrows)
                if (i + 2 < n && buf[i+1] == '[') {
                    switch(buf[i+2]) {
                        case 'A': ch = K_UPARROW; break;
                        case 'B': ch = K_DOWNARROW; break;
                        case 'C': ch = K_RIGHTARROW; break;
                        case 'D': ch = K_LEFTARROW; break;
                        default:  ch = 0; break;
                    }
                    i += 2; // Safely skip the processed sequence block
                } else {
                    ch = K_ESCAPE; // Genuine single ESC press
                }
            }
            else if (ch == 10 || ch == 13) ch = K_ENTER;
            else if (ch == 127)            ch = K_BACKSPACE;
            else if (ch == 9)              ch = K_TAB;
            else if (ch >= 1 && ch <= 26) { // Control key modifiers (Ctrl+A through Ctrl+Z)
                Key_Event(K_CTRL, true);
                key_registry[K_CTRL].is_pressed = yes;
                key_registry[K_CTRL].last_seen_time = current_time;
                ch += 96; // Normalize back to standard lowercase binding ascii
            }
        #endif
        
        // Dispatch clean character token directly to the engine
        if (ch > 0 && ch < 256) {
            if (!key_registry[ch].is_pressed) {
                Key_Event(ch, true);
                key_registry[ch].is_pressed = yes;
            }
            key_registry[ch].last_seen_time = current_time;
        }
    }

    decay:
        // Gracefully decay key-down state vectors over time
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
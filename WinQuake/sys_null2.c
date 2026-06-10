#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <time.h>      // Added: Uses standard ANSI clock() loops instead of GetTickCount/gettimeofday

// Standard platform-agnostic headers for directories and signals
#if defined(_MSC_VER) || defined(__MINGW32__)
    #include <direct.h>
    #define sys_mkdir(p) _mkdir(p)
#else
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <signal.h>
    #define sys_mkdir(p) mkdir(p, 0777)
#endif

#include "sx/scr.h"
#include "quakedef.h"

qboolean isDedicated;
int nostdout = 0;
char *basedir = ".";
char *cachedir = "/tmp";

cvar_t sys_linerefresh = {"sys_linerefresh", "0"};

// =======================================================================
// General routines
// =======================================================================

void Sys_DebugNumber(int y, int val)
{
}

void Sys_Printf(char *fmt, ...)
{
    va_list argptr;
    char text[1024];
    unsigned char *p;

    va_start(argptr, fmt);
    // Optimization 1: Use vsnprintf to safely prevent stack smashing/buffer overflows
    vsnprintf(text, sizeof(text), fmt, argptr);
    va_end(argptr);

    if (nostdout)
        return;

    static int console_cursor_x = 0;
    static int console_cursor_y = 0;
    ColorRGB white_color = {0, 255, 0};

    // Optimization 2: Ensure ALL render structures are completely instantiated before writing
    if (screen != NULL && screen->px != NULL && screen->color != NULL) 
    {
        for (p = (unsigned char *)text; *p; p++) 
        {
            char clean_char = *p & 0x7f;

            if (clean_char == '\n' || clean_char == '\r') 
            {
                console_cursor_x = 0;
                console_cursor_y++;
                if (console_cursor_y >= screen->h) 
                {
                    console_cursor_y = screen->h - 1;
                }
                continue;
            }
            if (clean_char == '\t') 
            {
                console_cursor_x += 4;
                if (console_cursor_x >= screen->w) {
                    console_cursor_x = 0;
                    console_cursor_y++;
                }
                continue;
            }

            if (clean_char >= 32 && clean_char < 127) 
            {
                if (console_cursor_x >= screen->w) 
                {
                    console_cursor_x = 0;
                    console_cursor_y++;
                }
                if (console_cursor_y >= screen->h) 
                {
                    console_cursor_y = screen->h - 1;
                }

                scr_putpx(screen, console_cursor_x, console_cursor_y, clean_char, white_color);
                console_cursor_x++;
            }
        }

        // Optimization 3: Only trigger an immediate screen update if the system is fully running
        // This avoids crashing during D_InitCaches/Host_Init boot logs.
        if (render_buf != NULL)
        {
            scr_draw(screen);
        }
    }
}

void Sys_Quit(void)
{
    Host_Shutdown();
    exit(0);
}

void Sys_Init(void)
{
}

void Sys_Error(char *error, ...)
{ 
    va_list argptr;
    char string[1024];
    
    va_start(argptr, error);
    vsprintf(string, error, argptr);
    va_end(argptr);
    fprintf(stderr, "Error: %s\n", string);
    Host_Shutdown();
    exit(1);
} 

void Sys_Warn(char *warning, ...)
{ 
    va_list argptr;
    char string[1024];
    
    va_start(argptr, warning);
    vsprintf(string, warning, argptr);
    va_end(argptr);
    fprintf(stderr, "Warning: %s\n", string);
} 

/*
============
Sys_FileTime
============
*/
int Sys_FileTime(char *path)
{
    FILE *f = fopen(path, "rb");
    if (f) {
        fclose(f);
        return 1; 
    }
    return -1;
}

void Sys_mkdir(char *path)
{
    sys_mkdir(path);
}

// Global lookup tracker since Quake tracks files by pseudo integer handles
#define MAX_HANDLES 64
static FILE *file_handles[MAX_HANDLES];

int Sys_FileOpenRead(char *path, int *handle)
{
    int i;
    for (i = 1; i < MAX_HANDLES; i++) {
        if (!file_handles[i]) {
            FILE *f = fopen(path, "rb");
            if (!f) return -1;
            
            file_handles[i] = f;
            *handle = i;
            
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            fseek(f, 0, SEEK_SET);
            return (int)size;
        }
    }
    return -1;
}

int Sys_FileOpenWrite(char *path)
{
    int i;
    for (i = 1; i < MAX_HANDLES; i++) {
        if (!file_handles[i]) {
            FILE *f = fopen(path, "wb+");
            if (!f) Sys_Error("Error opening write target: %s", path);
            file_handles[i] = f;
            return i;
        }
    }
    return -1;
}

int Sys_FileWrite(int handle, void *src, int count)
{
    if (handle <= 0 || handle >= MAX_HANDLES || !file_handles[handle]) return -1;
    return (int)fwrite(src, 1, count, file_handles[handle]);
}

void Sys_FileClose(int handle)
{
    if (handle <= 0 || handle >= MAX_HANDLES || !file_handles[handle]) return;
    fclose(file_handles[handle]);
    file_handles[handle] = NULL;
}

void Sys_FileSeek(int handle, int position)
{
    if (handle <= 0 || handle >= MAX_HANDLES || !file_handles[handle]) return;
    fseek(file_handles[handle], position, SEEK_SET);
}

int Sys_FileRead(int handle, void *dest, int count)
{
    if (handle <= 0 || handle >= MAX_HANDLES || !file_handles[handle]) return -1;
    return (int)fread(dest, 1, count, file_handles[handle]);
}

void Sys_DebugLog(char *file, char *fmt, ...)
{
    va_list argptr; 
    char data[1024];
    
    va_start(argptr, fmt);
    vsprintf(data, fmt, argptr);
    va_end(argptr);

    FILE *f = fopen(file, "a");
    if (f) {
        fprintf(f, "%s", data);
        fclose(f);
    }
}

void Sys_EditFile(char *filename)
{
}

double Sys_FloatTime(void)
{
    // Completely platform-agnostic precision ticker using standard C <time.h>
    static clock_t start_ticks = 0;
    if (!start_ticks) {
        start_ticks = clock();
        return 0.0;
    }
    return (double)(clock() - start_ticks) / CLOCKS_PER_SEC;
}

void Sys_LineRefresh(void)
{
}

char *Sys_ConsoleInput(void)
{
    return NULL; 
}

void Sys_HighFPPrecision(void) {}
void Sys_LowFPPrecision(void) {}

int main(int c, char **v)
{
    double time, oldtime, newtime;
    quakeparms_t parms;
    extern int vcrFile;
    extern int recording;
    int j;

#if !defined(_WIN32) && !defined(__MINGW32__)
    signal(SIGFPE, SIG_IGN);
#endif

    memset(file_handles, 0, sizeof(file_handles));
    memset(&parms, 0, sizeof(parms));

    COM_InitArgv(c, v);
    parms.argc = com_argc;
    parms.argv = com_argv;

    parms.memsize = 8 * 1024 * 1024;
    j = COM_CheckParm("-mem");
    if (j)
        parms.memsize = (int)(Q_atof(com_argv[j + 1]) * 1024 * 1024);
    parms.membase = malloc(parms.memsize);

    parms.basedir = basedir;

    Host_Init(&parms);
    Sys_Init();

    if (COM_CheckParm("-nostdout"))
        nostdout = 1;
    else {
        printf("ASCII Quake Engine Initialized (Strict Standard-C Target).\n");
    }

    oldtime = Sys_FloatTime() - 0.1;
    while (1)
    {
        newtime = Sys_FloatTime();
        time = newtime - oldtime;

        if (cls.state == ca_dedicated)
        {
            if (time < sys_ticrate.value && (vcrFile == -1 || recording))
            {
                // Pure cross-platform standard loop spin logic
                continue;
            }
            time = sys_ticrate.value;
        }

        if (time > sys_ticrate.value * 2)
            oldtime = newtime;
        else
            oldtime += time;

        Host_Frame(time);

        if (sys_linerefresh.value)
            Sys_LineRefresh();
    }
}

void Sys_MakeCodeWriteable(unsigned long startaddr, unsigned long length)
{
}
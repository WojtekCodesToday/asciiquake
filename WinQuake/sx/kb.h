
#if defined(_WIN32) || defined(__MINGW32__)
#include <windows.h>

static HANDLE hStdin;
static DWORD prev_mode;

void kb_init(void) {
    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hStdin, &prev_mode);
    // Set to window input mode: disable line/echo, enable raw processing
    SetConsoleMode(hStdin, ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT);
}

void kb_restore(void) {
    SetConsoleMode(hStdin, prev_mode);
}

// Windows-specific non-blocking input reader
int Windows_ReadInput(unsigned char *buf, int max) {
    DWORD count;
    INPUT_RECORD ir[32];
    int read_idx = 0;

    GetNumberOfConsoleInputEvents(hStdin, &count);
    if (count == 0) return 0;

    PeekConsoleInput(hStdin, ir, 32, &count);
    for (DWORD i = 0; i < count && read_idx < max; i++) {
        if (ir[i].EventType == KEY_EVENT && ir[i].Event.KeyEvent.bKeyDown) {
            WORD vk = ir[i].Event.KeyEvent.wVirtualKeyCode;
            
            // 1. Map Arrow keys with the high bit flag
            if (vk == VK_UP)         buf[read_idx++] = 0x80 | 'A';
            else if (vk == VK_DOWN)  buf[read_idx++] = 0x80 | 'B';
            else if (vk == VK_LEFT)  buf[read_idx++] = 0x80 | 'D';
            else if (vk == VK_RIGHT) buf[read_idx++] = 0x80 | 'C';
            
            else if (vk == VK_RETURN)  buf[read_idx++] = 13;   // Enter
            else if (vk == VK_BACK)    buf[read_idx++] = 127;  // Backspace
            else if (vk == VK_ESCAPE)  buf[read_idx++] = 27;   // Escape
            else if (vk == VK_TAB)     buf[read_idx++] = 9;    // Tab
            else if (vk == VK_CONTROL) buf[read_idx++] = K_CTRL;
            
            // 3. Fallback for normal character text typing
            else {
                char ascii = ir[i].Event.KeyEvent.uChar.AsciiChar;
                if (ascii != 0) {
                    buf[read_idx++] = ascii;
                }
            }
        }
    }
    FlushConsoleInputBuffer(hStdin);
    return read_idx;
}
#else
    #include <stdio.h>
    #include <stdlib.h>
    #include <unistd.h>
    #include <termios.h>
    #include <sys/select.h>
    #define SLEEP_MS(ms) usleep((ms) * 1000)
    static struct termios orig_termios;
    static int is_raw = 0;

    void kb_init(void) {
        tcgetattr(STDIN_FILENO, &orig_termios);
        struct termios newt = orig_termios;
    
        // Minimal raw mode: Disable canonical and echo only
        newt.c_lflag &= ~(ICANON | ECHO);
    
        // Set for non-blocking read
        newt.c_cc[VMIN] = 0;
        newt.c_cc[VTIME] = 0;
    
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        is_raw = 1;
    }

    void kb_restore(void) {
        if (!is_raw) return;
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
        is_raw = 0;
    }

    int _kbhit(void) {
        struct timeval tv = { 0L, 0L };
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(0, &fds);
        return select(1, &fds, NULL, NULL, &tv) > 0;
    }

    int _getch(void) {
        return getchar();
    }
#endif
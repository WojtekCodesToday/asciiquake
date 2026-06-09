
#if defined(_WIN32) || defined(__MINGW32__)
    // MSVCRT forward declarations (No windows.h needed)
    int _kbhit(void);
    int _getch(void);
    __attribute__((stdcall)) void Sleep(unsigned long dwMilliseconds);

    #define SLEEP_MS(ms) Sleep(ms)
    
    // Windows handles console input differently; no init/restore needed
    inline void kb_init(void) {}
    inline void kb_restore(void) {}
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
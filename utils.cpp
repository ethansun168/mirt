#include <expected>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <string>
#include <utility>
#include "constants.h"
#include "utils.h"

struct termios orig_termios;
void die(const char *s) {
    // Clear screen
    write(STDOUT_FILENO, "\x1b[2J", 4);
    // Move cursor to top left
    write(STDOUT_FILENO, "\x1b[H", 3);
    // Restore cursor
    write(STDOUT_FILENO, "\x1b[0 q", 6);
    perror(s);
    exit(1);
}

void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) {
        die("tcsetattr");
    }
    thinCursor();
}

void thinCursor() {
    write(STDOUT_FILENO, "\x1b[0 q", 6);
}

void thickCursor() {
    write(STDOUT_FILENO, "\x1b[2 q", 6);
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");
    atexit(disableRawMode);

    struct termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
    thickCursor();
}

std::expected<std::pair<int, int>, std::string> getCursorPosition() {
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
        return std::unexpected("Get cursor position failed");

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1)
            break;
        if (buf[i] == 'R')
            break;
        i++;
    }
    buf[i] = '\0';
    int rows, cols;
    if (buf[0] != '\x1b' || buf[1] != '[')
        return std::unexpected("Get cursor position failed");
    if (sscanf(&buf[2], "%d;%d", &rows, &cols) != 2)
        return std::unexpected("Get cursor position failed");
    return std::make_pair(rows, cols);
}

std::expected<std::pair<int, int>, std::string> getWindowSize() {
    struct winsize ws;
    int rows, cols;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return std::unexpected("Write failed");
        auto cursorPos = getCursorPosition();
        if (cursorPos.has_value()) {
            return cursorPos.value();
        }
        return std::unexpected(cursorPos.error());
    } else {
        return std::make_pair(ws.ws_row, ws.ws_col);
    }
}

std::string parseLine(const std::string& line) {
    int tabs = 0;
    std::string ret = "";
    for (const char ch : line) {
        if (ch == '\t') {
            ++tabs;
        }
        else {
            std::string tab = std::string(TAB_STOP, ' ');
            for (int i = 0; i < tabs; ++i) {
                ret += tab;
            }
            tabs = 0;
            ret += ch;
        }
    }
    return ret;
}

size_t firstNonWhitespace(const std::string& line) {
    for (size_t i = 0; i < line.length(); ++i) {
        if (!isspace(static_cast<unsigned char>(line[i]))) {
            return i;
        }
    }
    return 0;
}

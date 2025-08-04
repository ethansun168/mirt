#include <termios.h>
#include <stdarg.h>
#include <fstream>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <time.h>

#define CTRL_KEY(k) ((k) & 0x1f)
const int TAB_STOP = 8;

struct editorConfig {
    int cx, cy;
    int rx;
    int screenrows;
    int screencols;
    int rowOffset;
    int colOffset;
    std::vector<std::string> rows;
    std::vector<std::string> renders;
    std::string filename;
    std::string statusMsg;
    time_t statusMsgTime;
    struct termios orig_termios;
};
struct editorConfig E;

enum EditorKey {
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    PAGE_UP,
    PAGE_DOWN,
    HOME_KEY,
    END_KEY,
    DEL_KEY
};

void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(s);
    exit(1);
}


void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {
        die("tcsetattr");
    }
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
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
}

int editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN)
            die("read");
    }

    if (c == '\x1b') {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1)
            return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1)
            return '\x1b';
        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1)
                    return '\x1b';
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            }
            else {
                switch (seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
        }
        else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }
        return '\x1b';
    }
    else {
        return c;
    }
}

int getCursorPosition(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
        return -1;

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1)
            break;
        if (buf[i] == 'R')
            break;
        i++;
    }
    buf[i] = '\0';
    if (buf[0] != '\x1b' || buf[1] != '[')
        return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
        return -1;
    return 0;
}

int getWindowSize(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        return getCursorPosition(rows, cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

// Turn tabs into spaces
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

void editorOpen(const std::string& filename) {
    E.filename = filename;
    std::ifstream file(filename);
    if (!file.is_open()) {
        die("Failed to open file");
    }
    std::string line;
    while (getline(file, line)) {
        E.rows.push_back(line);
        E.renders.push_back(parseLine(line));
    }
    file.close();
}

void initEditor() {
    E.cx = 0;
    E.cy = 0;
    E.rx = 0;
    E.rowOffset = 0;
    E.colOffset = 0;
    E.filename = "[No Name]";
    E.statusMsgTime = 0;

    if (getWindowSize(&E.screenrows, &E.screencols) == -1)
        die("getWindowSize");
    E.screenrows -= 2;
}

int editorRowCxToRx(const std::string& row, int cx) {
    int rx = 0;
    for (int i = 0; i < cx; i++) {
        if (row[i] == '\t') {
            rx += (TAB_STOP - 1);
        }
        rx++;
    }
    return rx;
}

void editorScroll() {
  E.rx = E.cx;
  if (E.cy < E.rows.size()) {
    E.rx = editorRowCxToRx(E.rows[E.cy], E.cx);
  }

  if (E.cy < E.rowOffset) {
    E.rowOffset = E.cy;
  }
  if (E.cy >= E.rowOffset + E.screenrows) {
    E.rowOffset = E.cy - E.screenrows + 1;
  }
  if (E.rx < E.colOffset) {
    E.colOffset = E.rx;
  }
  if (E.rx >= E.colOffset + E.screencols) {
    E.colOffset = E.rx - E.screencols + 1;
  }
}

void editorDrawRows(std::string& str) {
    for (int y = 0; y < E.screenrows; y++) {
        int filerow = y + E.rowOffset;

        if (filerow >= E.rows.size()) {
            if (E.rows.size() == 0 && y == E.screenrows / 3) {
                std::string welcome = "Welcome to Gamestrim Editor -- version 0.0.1";
                int padding = (E.screencols - welcome.length()) / 2;
                if (padding) {
                    str += "~";
                    padding--;
                }
                while (padding--) {
                    str += " ";
                }
                str += welcome;
            }
            else {
                str += "~";
            }
        }
        else {
            int len = E.renders[filerow].length() - E.colOffset;
            len = std::max(0, len);
            len = std::min(len, E.screencols);
            if (len != 0) {
                str += E.renders[filerow].substr(E.colOffset, len);
            }
        }

        str += "\x1b[K";
        str += "\r\n";
    }
}

void editorDrawStatusBar(std::string& str) {
    str += "\x1b[7m";
    std::string status = "";
    status += E.filename.substr(0, 20) + " - " + std::to_string(E.rows.size()) + " lines";

    std::string rstatus = std::to_string(E.cy + 1) + "/" + std::to_string(E.rows.size());
    while (status.length() < E.screencols) {
        if (E.screencols - status.length() == rstatus.length()) {
            status += rstatus;
            break;
        }
        status += " ";
    }
    str += status;
    str += "\x1b[m";
    str += "\r\n";
}

void editorDrawMessageBar(std::string& str) {
    str += "\x1b[K";
    int msglen = E.statusMsg.length();
    if (msglen > E.screencols) msglen = E.screencols;
    if (msglen && time(NULL) - E.statusMsgTime < 5)
        str += E.statusMsg;
}

void editorRefreshScreen() {
    editorScroll();

    std::string str;
    str += "\x1b[?25l";
    str += "\x1b[H";
    editorDrawRows(str);
    editorDrawStatusBar(str);
    editorDrawMessageBar(str);

    // Draw cursor
    str += "\x1b[" + std::to_string(E.cy - E.rowOffset + 1) + ";" + std::to_string(E.rx - E.colOffset + 1) + "H";

    str += "\x1b[?25h";
    write(STDOUT_FILENO, str.c_str(), str.length());
}

void editorSetStatusMessage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char buf[80];
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  E.statusMsg = buf;
  E.statusMsgTime = time(NULL);
}

void editorMoveCursor(int key) {
    switch (key) {
        case ARROW_LEFT:
        case 'h': {
            if (E.cx != 0) {
                E.cx--;
            }
            break;
        }
        case ARROW_RIGHT:
        case 'l': {
            if (E.cy < E.rows.size() && E.cx < E.rows[E.cy].length()) {
                E.cx++;
            }
            break;
        }
        case ARROW_UP:
        case 'k': {
            if (E.cy != 0) {
                E.cy--;
            }
            break;
        }
        case ARROW_DOWN:
        case 'j': {
            if (E.cy < E.rows.size() - 1) {
                E.cy++;
            }
            break;
        }
    }
    // Snap cursor to end of line
    int rowLen = E.cy < E.rows.size() ? E.rows[E.cy].length() : 0;
    if (E.cx > rowLen) {
        E.cx = rowLen;
    }

}
void editorProcessKeypress() {
    int c = editorReadKey();
    switch (c) {
        case CTRL_KEY('q'):
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;

        case HOME_KEY:
            E.cx = 0;
            break;
        case END_KEY:
            if (E.cy < E.rows.size())
                E.cx = E.rows[E.cy].length();

            break;
        case PAGE_UP:
        case PAGE_DOWN: {
            if (c == PAGE_UP) {
                E.cy = E.rowOffset;
            } else if (c == PAGE_DOWN) {
                E.cy = E.rowOffset + E.screenrows - 1;
                if (E.cy > E.rows.size()) E.cy = E.rows.size();
            }
            int times = E.screenrows;
            while (times--)
            editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
        }
        break;
        case 'h':
        case 'j':
        case 'k':
        case 'l':
        case ARROW_LEFT:
        case ARROW_RIGHT:
        case ARROW_UP:
        case ARROW_DOWN:
        editorMoveCursor(c);
        break;
    }
}

int main(int argc, char** argv) {
    enableRawMode();
    initEditor();
    if (argc >= 2) {
        editorOpen(argv[1]);
    }
    editorSetStatusMessage("HELP: Ctrl-Q = quit");
    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <fstream>
#include "editor.h"
#include "constants.h"
#include "utils.h"

Editor::Editor() : cx{0}, cy{0}, rx{0}, rowOffset{0}, colOffset{0}, filename{"[No Name]"}, statusMsgTime{0} {
    if (getWindowSize(&screenrows, &screencols) == -1)
        die("getWindowSize");
    screenrows -= 2;
}

int Editor::readKey() {
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

void Editor::appendRow(const std::string& line) {
    rows.push_back(line);
    renders.push_back(parseLine(line));
}

void Editor::openFile(const std::string& filename) {
    this->filename = filename;
    std::ifstream file(filename);
    if (!file.is_open()) {
        die("Failed to open file");
    }
    std::string line;
    while (getline(file, line)) {
        appendRow(line);
    }
    file.close();
}

int Editor::rowCxToRx(const std::string& row, int cx) {
    int rx = 0;
    for (int i = 0; i < cx; i++) {
        if (row[i] == '\t') {
            rx += (TAB_STOP - 1);
        }
        rx++;
    }
    return rx;
}

void Editor::insertChar(int c) {
    if (cy == rows.size()) {
        appendRow("");
    }
    rows[cy].insert(rows[cy].begin() + cx, c);
    renders[cy] = parseLine(rows[cy]);
    cx++;
}

void Editor::scroll() {
  rx = cx;
  if (cy < rows.size()) {
    rx = rowCxToRx(rows[cy], cx);
  }

  if (cy < rowOffset) {
    rowOffset = cy;
  }
  if (cy >= rowOffset + screenrows) {
    rowOffset = cy - screenrows + 1;
  }
  if (rx < colOffset) {
    colOffset = rx;
  }
  if (rx >= colOffset + screencols) {
    colOffset = rx - screencols + 1;
  }
}

void Editor::drawRows(std::string& str) {
    for (int y = 0; y < screenrows; y++) {
        int filerow = y + rowOffset;

        if (filerow >= rows.size()) {
            if (rows.size() == 0 && y == screenrows / 3) {
                std::string welcome = "Welcome to Gamestrim Editor -- version 0.0.1";
                int padding = (screencols - welcome.length()) / 2;
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
            int len = renders[filerow].length() - colOffset;
            len = std::max(0, len);
            len = std::min(len, screencols);
            if (len != 0) {
                str += renders[filerow].substr(colOffset, len);
            }
        }

        str += "\x1b[K";
        str += "\r\n";
    }
}

void Editor::drawStatusBar(std::string& str) {
    str += "\x1b[7m";
    std::string status = "";
    status += filename.substr(0, 20) + " - " + std::to_string(rows.size()) + " lines";

    std::string rstatus = std::to_string(cy + 1) + "/" + std::to_string(rows.size());
    while (status.length() < screencols) {
        if (screencols - status.length() == rstatus.length()) {
            status += rstatus;
            break;
        }
        status += " ";
    }
    str += status;
    str += "\x1b[m";
    str += "\r\n";
}

void Editor::drawMessageBar(std::string& str) {
    str += "\x1b[K";
    int msglen = statusMsg.length();
    if (msglen > screencols) msglen = screencols;
    if (msglen && time(NULL) - statusMsgTime < 5)
        str += statusMsg;
}

void Editor::refreshScreen() {
    scroll();

    std::string str;
    str += "\x1b[?25l";
    str += "\x1b[H";
    drawRows(str);
    drawStatusBar(str);
    drawMessageBar(str);

    // Draw cursor
    str += "\x1b[" + std::to_string(cy - rowOffset + 1) + ";" + std::to_string(rx - colOffset + 1) + "H";

    str += "\x1b[?25h";
    write(STDOUT_FILENO, str.c_str(), str.length());
}

void Editor::setStatusMessage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char buf[80];
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  statusMsg = buf;
  statusMsgTime = time(NULL);
}

void Editor::moveCursor(int key) {
    switch (key) {
        case ARROW_LEFT:
        case 'h': {
            if (cx != 0) {
                cx--;
            }
            break;
        }
        case ARROW_RIGHT:
        case 'l': {
            if (cy < rows.size() && cx < rows[cy].length()) {
                cx++;
            }
            break;
        }
        case ARROW_UP:
        case 'k': {
            if (cy != 0) {
                cy--;
            }
            break;
        }
        case ARROW_DOWN:
        case 'j': {
            if (cy < rows.size()) {
                cy++;
            }
            break;
        }
    }
    // Snap cursor to end of line
    int rowLen = cy < rows.size() ? rows[cy].length() : 0;
    if (cx > rowLen) {
        cx = rowLen;
    }
}

void Editor::save() {

}

void Editor::processKeyPress() {
    int c = readKey();
    switch (c) {
        case '\r':
            /* TODO */
            break;
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
        
        case CTRL_KEY('s'):
            save();
            break;

        case HOME_KEY:
            cx = 0;
            break;
        case END_KEY:
            if (cy < rows.size())
                cx = rows[cy].length();
            break;

        case BACKSPACE:
        case CTRL_KEY('h'):
        case DEL_KEY:
            /* TODO */
            break;
        case PAGE_UP:
        case PAGE_DOWN: {
            if (c == PAGE_UP) {
                cy = rowOffset;
            } else if (c == PAGE_DOWN) {
                cy = rowOffset + screenrows - 1;
                if (cy > rows.size()) cy = rows.size();
            }
            int times = screenrows;
            while (times--)
            moveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            break;
        }
        // case 'h':
        // case 'j':
        // case 'k':
        // case 'l':
        case ARROW_LEFT:
        case ARROW_RIGHT:
        case ARROW_UP:
        case ARROW_DOWN:
            moveCursor(c);
        break;

        case CTRL_KEY('l'):
            case '\x1b':
            break;

        default:
            insertChar(c);
            break;
    }
}
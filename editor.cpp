#include <stdio.h>
#include <cassert>
#include <format>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <fstream>
#include "editor.h"
#include "constants.h"
#include "utils.h"

Editor::Editor() :
    cx{0},
    cy{0},
    rx{0},
    rowOffset{0},
    colOffset{0},
    filename{""},
    statusMsgTime{0},
    dirty{false},
    mode{Mode::NORMAL},
    lineNumberWidth{0},
    lastCx{0}
{
    if (getWindowSize(&screenrows, &screencols) == -1)
        die("getWindowSize");
    screenrows -= 2;
    options["number"] = false;
    options["relativenumber"] = false;
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

void Editor::insertNewline() {
    assert(cx >= 0);
    if (cx == 0) {
        rows.insert(rows.begin() + cy, "");
        renders.insert(renders.begin() + cy, "");
    }
    else {
        // Split line at cursor
        std::string lhs = rows[cy].substr(0, cx);
        std::string rhs = rows[cy].substr(cx);
        rows[cy] = lhs;
        rows.insert(rows.begin() + cy + 1, rhs);
        renders[cy] = parseLine(rows[cy]);
        renders.insert(renders.begin() + cy + 1, parseLine(rows[cy + 1]));
    }
    ++cy;
    cx = 0;
    lastCx = cx;
}

void Editor::insertChar(int c) {
    if (cy == rows.size()) {
        appendRow("");
    }
    rows[cy].insert(rows[cy].begin() + cx, c);
    renders[cy] = parseLine(rows[cy]);
    cx++;

    lastCx = cx - 1;
    dirty = true;
}

void Editor::deleteChar() {
    if (cy == rows.size()) {
        return;
    }
    if (cx == 0 && cy == 0) {
        return;
    }

    if (cx > 0) {
        rows[cy].erase(rows[cy].begin() + cx - 1);
        renders[cy] = parseLine(rows[cy]);
        --cx;
    }
    else {
        // Concatenate with previous row
        cx = rows[cy - 1].length();
        std::string prev = rows[cy - 1];
        rows[cy - 1] += rows[cy];
        renders[cy - 1] = parseLine(rows[cy - 1]);
        rows.erase(rows.begin() + cy);
        renders.erase(renders.begin() + cy);        
        --cy;
    }
    lastCx = std::max(0, cx - 1);
    dirty = true;
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

  // Account for linenumberwidth
  int textCols = screencols - lineNumberWidth;
  if (rx < colOffset) {
    colOffset = rx;
  }
  if (rx > colOffset + textCols - 1) {
    colOffset = rx - textCols + 1;
  }
}

void Editor::drawRows(std::string& str) {
    lineNumberWidth = (options["number"] || options["relativenumber"])
        ? std::max(4, (int)std::to_string(std::max(1, (int)rows.size())).size() + 1)
        : 0;

    for (int y = 0; y < screenrows; y++) {
        int filerow = y + rowOffset;

        if (filerow >= rows.size()) {
            str += std::string(lineNumberWidth, ' '); 
            if (!dirty && filename.empty() && rows.size() == 1 && rows[0].empty() && y == screenrows / 3) {
                std::string welcome = "Welcome to mirt -- version 0.0.1";
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
            if (options["number"] || options["relativenumber"]) {
                int relativeNumber = std::abs(filerow - cy);
                std::string lineNumber;
                if (options["number"] && options["relativenumber"]) {
                    if (relativeNumber == 0) {
                        lineNumber = std::to_string(filerow + 1);
                    }
                    else {
                        lineNumber = std::to_string(relativeNumber);
                    }
                }
                else if (!options["number"] && options["relativenumber"]) {
                    lineNumber = std::to_string(relativeNumber);
                }
                else if (options["number"] && !options["relativenumber"]) {
                    lineNumber = std::to_string(filerow + 1);
                }

                // Dim line numbers
                str += "\x1b[2m";

                if (relativeNumber == 0) {
                    // Unindent current line
                    str += lineNumber + std::string(lineNumberWidth - lineNumber.size() - 1, ' ') + " ";
                }
                else {
                    str += std::string(lineNumberWidth - lineNumber.size() - 1, ' ') + lineNumber + " ";
                }
                // Reset to normal
                str += "\x1b[22m";
            }


            int textCols = screencols - lineNumberWidth;
            int len = renders[filerow].length() - colOffset;
            len = std::max(0, len);
            len = std::min(len, textCols);
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
    std::string status = std::format("{:.20} - {} lines {}",
        filename.empty() ? "[No Name]" : filename,
        rows.size(),
        dirty ? "(modified)" : ""
    );
    std::string rstatus = std::format("{}, {}", cy + 1, cx + 1);
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
    str += "\x1b[" + std::to_string(cy - rowOffset + 1) + ";" 
        + std::to_string(rx - colOffset + 1 + lineNumberWidth) + "H";


    str += "\x1b[?25h";
    write(STDOUT_FILENO, str.c_str(), str.length());
}

void Editor::setStatusMessage(const std::string& msg) {
    statusMsg = msg;
    statusMsgTime = time(NULL);
}

void Editor::moveCursor(int key, Mode mode) {
    switch (key) {
        case ARROW_LEFT:
        case 'h': {
            if (cx != 0) {
                cx--;
            } else if (cy > 0 && mode == Mode::INSERT) {
                cy--;
                cx = rows[cy].length();
            }
            lastCx = cx;
            break;
        }
        case ARROW_RIGHT:
        case 'l': {
            switch (mode) {
                case Mode::NORMAL: {
                    if (cy < rows.size() && cx < (int) rows[cy].length() - 1) {
                        cx++;
                    }
                    break;
                }
                case Mode::INSERT:
                    if (cy < rows.size() && cx < rows[cy].length()) {
                        cx++;
                    } else if (cy < rows.size() - 1 && cx == rows[cy].length()) {
                        cy++;
                        cx = 0;
                    }
                    break;
            }
            lastCx = cx;
            break;
        }
        case ARROW_UP:
        case 'k': {
            if (cy != 0) {
                cy--;
            }
            switch (mode) {
                case Mode::NORMAL:
                    cx = std::max(0, std::min(lastCx, (int)rows[cy].size() - 1));
                    break;
                case Mode::INSERT:
                    cx = std::min(lastCx, (int)rows[cy].size());
                    break;
            }
            break;
        }
        case ARROW_DOWN:
        case 'j': {
            if (cy < rows.size() - 1) {
                cy++;
            }
            switch (mode) {
                case Mode::NORMAL:
                    cx = std::max(0, std::min(lastCx, (int)rows[cy].size() - 1));
                    break;
                case Mode::INSERT:
                    cx = std::min(lastCx, (int)rows[cy].size());
                    break;
            }

            break;
        }
    }
    // Snap cursor to end of line
    int rowLen = cy < rows.size() ? rows[cy].length() : 0;
    if (cx > rowLen) {
        cx = rowLen;
    }

    // Normal mode end of line
    if (mode == Mode::NORMAL && cx == rows[cy].length() - 1) {
        cx++;
        refreshScreen();
        cx--;
        refreshScreen();
    }

    assert(cx >= 0);
    assert(cy >= 0);
}

std::string Editor::prompt(const std::string& prompt) {
    std::string input = "";
    int rows, cols;
    size_t cursorPos = 0;
    getWindowSize(&rows, &cols);

    size_t placeholderPos = prompt.find("{}");
    std::string before = prompt.substr(0, placeholderPos);
    std::string after = placeholderPos != std::string::npos
                        ? prompt.substr(placeholderPos + 2)
                        : "";

    while (true) {
        refreshScreen();

        // Move to bottom line and clear it
        std::string statusMessage = std::format("\x1b[{};1H\x1b[K", rows);

        // Print prompt
        statusMessage += before;
        statusMessage += input;
        statusMessage += after;

        // Move cursor: start of prompt + position inside input
        statusMessage += std::format("\x1b[{};{}H", rows, before.size() + cursorPos + 1);

        // Set cursor shape
        statusMessage += "\x1b[0 q";

        // Write it all at once
        write(STDOUT_FILENO, statusMessage.c_str(), statusMessage.size());

        int c = readKey();
        if (c == CTRL_KEY('h') || c == BACKSPACE) {
            if (cursorPos > 0) {
                input.erase(cursorPos - 1, 1);
                cursorPos--;
            }
        }
        else if (c == DEL_KEY) {
            if (cursorPos < input.size()) {
                input.erase(cursorPos, 1);
            }
        }
        else if (c == '\x1b') {
            setStatusMessage("");
            thickCursor();
            return "";
        }
        else if (c == '\r') {
            if (input.length() > 0) {
                thickCursor();
                return input;
            } 
        }
        else if (c == ARROW_LEFT) {
            if (cursorPos > 0) cursorPos--;
        }
        else if (c == ARROW_RIGHT) {
            if (cursorPos < input.length()) cursorPos++;
        }
        else if (!iscntrl(c) && c < 128) {
            input.insert(cursorPos, 1, (char)c);
            ++cursorPos;
        }
    }
}

bool Editor::save() {
    if (filename.empty()) {
        filename = prompt("Save as: {} (ESC to cancel)");
        if (filename.empty()) {
            setStatusMessage("Save aborted");
            return false;
        }
    }
    std::string data = "";
    for (const std::string& row : rows) {
        data += row + "\n";
    }
    int fd = open(filename.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd == -1) {
        setStatusMessage(std::format("Can't save! I/O error: {}", strerror(errno)));
        return false;
    }

    if (ftruncate(fd, data.length()) != -1) {
        setStatusMessage(std::format("{} bytes written to disk", data.length()));
        dirty = false;
        write(fd, data.c_str(), data.length());
    }
    close(fd);
    return true;
}

void Editor::setCommandHandler(const std::string& subCommand) {
    if (subCommand == "number" || subCommand == "nu") {
        options["number"] = true;
    }
    else if (subCommand == "nonumber" || subCommand == "nonu") {
        options["number"] = false;
    }
    if (subCommand == "relativenumber" || subCommand == "rnu") {
        options["relativenumber"] = true;
    }
    else if (subCommand == "norelativenumber" || subCommand == "nornu") {
        options["relativenumber"] = false;
    }
    else if (subCommand.starts_with("tabstop=")) {
        int tabStop = std::stoi(subCommand.substr(8));
        if (tabStop > 0) {
            TAB_STOP = tabStop;
            // Rerender the render rows
            for (int i = 0; i < renders.size(); ++i) {
                renders[i] = parseLine(rows[i]);
            }
            refreshScreen();
        }
    }
    else {
        setStatusMessage(std::format("Unknown command: {}", subCommand));
    }
}

// Returns new cursor position after moving `w` motion
void Editor::wordMotion(int n) {
    auto isKeywordChar = [](char c) {
        return std::isalnum((unsigned char)c) || c == '_';
    };

    for (int i = 0; i < n; ++i) {

        // If past end of file
        if (cy >= rows.size())
            return;

        std::string &row = rows[cy];

        // --- 1. If we're in the middle of a word, skip to its end ---
        if (!row.empty() && cx < (int)row.size() && !std::isspace((unsigned char)row[cx])) {
            bool currIsKeyword = isKeywordChar(row[cx]);
            while (cx < (int)row.size() && isKeywordChar(row[cx]) == currIsKeyword) {
                cx++;
            }
        }

        // --- 2. If we're at or past end of line, go to next line ---
        if (cx >= (int)row.size()) {
            cy++;
            cx = 0;

            // **Vim behavior**: Stop if the new line is empty
            if (cy < (int)rows.size() && rows[cy].empty())
                continue;

        }

        // --- 3. Skip whitespace to get to next word ---
        while (cy < (int)rows.size() && std::isspace((unsigned char)rows[cy][cx])) {
            cx++;
            if (cx >= (int)rows[cy].size()) {
                cy++;
                cx = 0;
                // Stop if it's an empty line
                if (cy < (int)rows.size() && rows[cy].empty())
                    continue;
            }
        }
    }

}


void Editor::processNormalKey(int c) {
    switch(c) {
        case ':': {
            std::string command = prompt(":{}");
            if (command.empty()) {
                setStatusMessage("Aborted");
                return;
            }
            if (command == "w") {
                save();
            }
            else if (command == "wq") {
                if (!save()) {
                    return;
                }
                write(STDOUT_FILENO, "\x1b[2J", 4);
                write(STDOUT_FILENO, "\x1b[H", 3);
                exit(0);
            }
            else if (command == "q!") {
                write(STDOUT_FILENO, "\x1b[2J", 4);
                write(STDOUT_FILENO, "\x1b[H", 3);
                exit(0);
            }
            else if (command == "q") {
                if (dirty) {
                    setStatusMessage("Unsaved changes. (add ! to override)");
                    return;
                }
                else {
                    write(STDOUT_FILENO, "\x1b[2J", 4);
                    write(STDOUT_FILENO, "\x1b[H", 3);
                    exit(0);
                }
            }
            else if (command.starts_with("set ")) {
                std::string subCommand = command.substr(4);
                setCommandHandler(subCommand); 
            }
            break;
        }
        case 'h':
        case 'j':
        case 'k':
        case 'l':
            moveCursor(c, mode);
            break;
        
        case 'a':
            ++cx;
            lastCx = cx;
        case 'i':
            setInsert();
            break;
        case 'o': {
            processInsertKey(END_KEY);
            lastCx = 0;
            insertNewline();
            setInsert();
            break;
        }
            
        case '0':
            cx = 0;
            lastCx = 0;
            break;
        case END_KEY:
        case '$': {
            if (cy < rows.size()) {
                cx = std::max(0, (int) rows[cy].length() - 1);
                lastCx = cx;
                if (cx == rows[cy].length() - 1) {
                    cx++;
                    refreshScreen();
                    cx--;
                    refreshScreen();
                }
            }
            break;
        }
        case '_': {
            size_t first = firstNonWhitespace(rows[cy]);
            cx = first;
            lastCx = cx;
            break;
        }
        case 'w': {
            wordMotion(1);
            lastCx = cx;
        }
    }
}

void Editor::processInsertKey(int c) {
    switch (c) {
        case '\r':
            insertNewline();
            break;

        case BACKSPACE:
        case CTRL_KEY('h'):
        case DEL_KEY:
            if (c == DEL_KEY) moveCursor(ARROW_RIGHT, mode);
            deleteChar();
            break;

        case CTRL_KEY('l'):
            break;
        case '\x1b':
            setNormal();
            if (cx > 0) {
                --cx;
            }
            break;

        case END_KEY:
            if (cy < rows.size())
                cx = std::max(0, (int) rows[cy].length());
                lastCx = cx;
            break;

        default:
            insertChar(c);
            break;
    }
}

void Editor::processKeyPress() {
    int c = readKey();
    // Common between both modes
    switch (c) {
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
            moveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN, mode);
            return;
        }

        case ARROW_LEFT:
        case ARROW_RIGHT:
        case ARROW_UP:
        case ARROW_DOWN:
            moveCursor(c, mode);
            return;

        case HOME_KEY:
            cx = 0;
            lastCx = 0;
            return;
    }
    switch (mode) {
        case Mode::NORMAL:
            processNormalKey(c);
            break;
        case Mode::INSERT:
            processInsertKey(c);
            break;
    }
}

void Editor::appendIfBufferEmpty() {
    if (rows.empty()) {
        appendRow("");
    }
}

void Editor::setInsert() {
    thinCursor();
    mode = Mode::INSERT;
    setStatusMessage("-- INSERT --");
}

void Editor::setNormal() {
    thickCursor();
    mode = Mode::NORMAL;
    setStatusMessage("-- NORMAL --");
}

void Editor::config() {
    std::string filename = ".mirtrc";
    std::ifstream file(filename);
    if (!file.is_open()) {
        return;
    }
    std::string command;
    while (getline(file, command)) {
        if (command.starts_with("set ")) {
            std::string subCommand = command.substr(4);
            setCommandHandler(subCommand);
        }
    }

    file.close();
}
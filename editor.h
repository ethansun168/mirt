#pragma once
#include <vector>
#include <string>
#include <termios.h>

class Editor {
private:
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
    bool dirty;

    enum EditorKey {
        BACKSPACE = 127,
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

    int readKey();
    void appendRow(const std::string& line);
    int rowCxToRx(const std::string& row, int cx);
    void insertChar(int c); // Insert character at cy, cx
    void deleteChar(); // Delete character at cy, cx
    void insertNewline(); // Insert newline at cy
    void scroll();
    void drawRows(std::string& str);
    void drawStatusBar(std::string& str);
    void drawMessageBar(std::string& str);
    void moveCursor(int key);
    void save();

public:
    Editor();
    void openFile(const std::string& filename);
    void refreshScreen();
    void processKeyPress();
    void setStatusMessage(const std::string& msg);
};
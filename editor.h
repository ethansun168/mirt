#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include <termios.h>

class Editor {
private:
    enum class Mode {
        NORMAL,
        INSERT
    };

    int cx, cy;
    int rx;
    int lastCx;
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
    Mode mode;
    int lineNumberWidth;
    std::unordered_map<std::string, bool> options;
    std::vector<char> ops;

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

    enum class WordMotionTarget {
        START,
        END
    };

    int readKey();
    void appendRow(const std::string& line);
    int rowCxToRx(const std::string& row, int cx);

    // Insert character at cy, cx
    void insertChar(int c);

    // Delete character at cy, cx
    void deleteChar();

    // Insert newline at cy
    void insertNewline();

    void scroll();
    void drawRows(std::string& str);
    void drawStatusBar(std::string& str);
    void drawMessageBar(std::string& str);
    void moveCursor(int key, Mode mode);
    
    // Returns if save was successful
    bool save();

    // Prompt the user for input. Returns the user input
    std::string prompt(const std::string& prompt);

    void processInsertKey(int c);
    void processNormalKey(int c);

    void setInsert();
    void setNormal();

    void setCommandHandler(const std::string& subCommand);

    // Move cursor in direction `dir` by `n` words
    void wordMotion(int n, bool dir, WordMotionTarget target);

public:
    Editor();
    void openFile(const std::string& filename);
    void refreshScreen();
    void processKeyPress();
    void setStatusMessage(const std::string& msg);
    void appendIfBufferEmpty();

    // Open .mirtrc in same dir as mirt executable
    void config();
};
#pragma once
#include <string>
void die(const char *s);
void disableRawMode();
void enableRawMode();
int getCursorPosition(int *rows, int *cols);
int getWindowSize(int *rows, int *cols);

void thinCursor();
void thickCursor();

// Turn tabs into spaces
std::string parseLine(const std::string& line);
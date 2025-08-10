#pragma once
#include <string>
#include <expected>
void die(const char *s);
void disableRawMode();
void enableRawMode();
std::expected<std::pair<int, int>, std::string> getCursorPosition();
std::expected<std::pair<int, int>, std::string> getWindowSize();

void thinCursor();
void thickCursor();

// Turn tabs into spaces
std::string parseLine(const std::string& line);

size_t firstNonWhitespace(const std::string& line);

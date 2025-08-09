#pragma once
#include <unordered_set>
#include <vector>
#include <unordered_map>

#define CTRL_KEY(k) ((k) & 0x1f)
inline int TAB_STOP = 8;

const std::vector<char> openBrackets = {'{', '(', '['};
const std::vector<char> closedBrackets = {'}', ')', ']'};
const std::unordered_map<char, char> bracketMatches = {
    { '(', ')' },
    { '{', '}' },
    { '[', ']' },
    { ')', '(' },
    { '}', '{' },
    { ']', '[' },
};
const std::unordered_set<char> operators = {
    'd',
    'c',
    'y',
    '1',
    '2',
    '3',
    '4',
    '5',
    '6',
    '7',
    '8',
    '9',
};

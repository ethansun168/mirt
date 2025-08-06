#pragma once
#include <unordered_set>

#define CTRL_KEY(k) ((k) & 0x1f)
inline int TAB_STOP = 8;
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
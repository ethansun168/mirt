#pragma once
#include <unordered_set>

#define CTRL_KEY(k) ((k) & 0x1f)
inline int TAB_STOP = 8;
const std::unordered_set<char> operators = {
    'd',
    'c',
    'y',
};
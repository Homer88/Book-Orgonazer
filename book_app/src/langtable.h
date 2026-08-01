#pragma once

struct LangEntry {
    const char *display;
    const char *stored;
    int code;
};

inline const LangEntry kLangTable[] = {
    {"Ассемблер", "assembler", 1},
    {"C", "c", 10},
    {"C++", "c++", 100},
    {"C#", "c#", 1000},
    {"Java", "Java", 10000},
    {"Pascal", "Pascal", 100000},
    {"Basic", "Basic", 1000000},
    {"SQL", "SQL", 10000000},
    {"Другое", "Другое", 100000000},
};
inline const int kLangCount = int(sizeof(kLangTable) / sizeof(kLangTable[0]));

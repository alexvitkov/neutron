#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <strsafe.h>

#include "util.h"

HANDLE console;
CONSOLE_SCREEN_BUFFER_INFO csbi;

const char PATH_SEPARATOR = ';';

Red red;
Dim dim;
ResetStyle resetstyle;

void init_utils() {
    console = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(console, &csbi);
}


std::ostream& operator<<(std::ostream& o, Red& r) {
    SetConsoleTextAttribute(console, 12 | csbi.wAttributes & 0x00F0);
    return o;
}

std::ostream& operator<<(std::ostream& o, ResetStyle& r) {
    SetConsoleTextAttribute(console, csbi.wAttributes);
    return o;
}

std::ostream& operator<<(std::ostream& o, Dim& r) {
    SetConsoleTextAttribute(console, 8 | csbi.wAttributes & 0x00F0);
    return o;
}

int exec(const char* programname, arr<const char*>& args) {
    assert(!"Not implemented");
    return 1;
}

wchar_t* env(const wchar_t* var) {
    // TODO long variables won't fit
    const int buf_size = 1024;
    wchar_t* buf = (wchar_t*)malloc(sizeof(wchar_t*) * buf_size);

    DWORD length = GetEnvironmentVariableW(var, buf, buf_size);

    if (length) {
        buf[length] = 0;
        return buf;
    }
    else {
        free(buf);
        return nullptr;
    }
}

bool util_read_dir(const wchar_t* dirname, arr<wchar_t*>& out) {
    WIN32_FIND_DATAW data;
    HANDLE f = FindFirstFileW(dirname, &data);

    if (f == INVALID_HANDLE_VALUE) {
        return false;
    }

    do  {
        wprintf(L"%s\n", data.cFileName);
    } while (FindNextFileW(f, &data));
   
    FindClose(f);
    return true;
}
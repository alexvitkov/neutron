#include "util.h"

#ifdef _WINDOWS
#include "Windows.h"

HANDLE console;
CONSOLE_SCREEN_BUFFER_INFO csbi;
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

Red red;
ResetStyle resetstyle;
Dim dim;

void init_utils() {
#ifdef _WINDOWS
	console = GetStdHandle(STD_OUTPUT_HANDLE);
	GetConsoleScreenBufferInfo(console, &csbi);
#endif
}


std::ostream& operator<<(std::ostream& o, Red& r) {
#ifdef _WINDOWS
	SetConsoleTextAttribute(console, 12 | csbi.wAttributes & 0x00F0);
#else
	o << "\e[31;1m";
#endif
	return o;
}

std::ostream& operator<<(std::ostream& o, ResetStyle& r) {
#ifdef _WINDOWS
	SetConsoleTextAttribute(console, csbi.wAttributes);
#else
	o << "\e[0m";
#endif
	return o;
}

std::ostream& operator<<(std::ostream& o, Dim& r) {
#ifdef _WINDOWS
	SetConsoleTextAttribute(console, 8 | csbi.wAttributes & 0x00F0);
#else
	o << "\e[2m";
#endif
	return o;
}


int exec(const char* programname, arr<const char*>& args) {
#ifdef _WINDOWS
    assert(!"Not implemented");
#else
    int child_pid = fork();
    if (child_pid) {
        int status;
        waitpid(child_pid, &status, 0);
        return WEXITSTATUS(status);
    }
    else {
        args.push(0);
        execvp(programname, (char**)args.buffer);
        assert(!"exec failed");
    }
#endif
}

const char* env(const char* var) {
#ifdef _WINDOWS
    const int buf_size = 1024;
    char* buf = (char*)malloc(buf_size);

    DWORD length = GetEnvironmentVariable(var, buf, buf_size - 1);

    if (length) {
        buf[length] = 0;
        return buf;
    }
    else {
        free(buf);
        return nullptr;
    }
#else
    return getenv(var);
#endif
}

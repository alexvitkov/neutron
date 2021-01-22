#include "util.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

Red red;
Dim dim;
ResetStyle resetstyle;

const char PATH_SEPARATOR = ':';

// this does nothing on posix
void init_utils() {
}

std::ostream& operator<<(std::ostream& o, Red& r) {
    o << "\e[0m";
    o << "\e[31;1m";
    return o;
}

std::ostream& operator<<(std::ostream& o, ResetStyle& r) {
    o << "\e[0m";
    return o;
}

std::ostream& operator<<(std::ostream& o, Dim& r) {
    o << "\e[0m";
    o << "\e[2m";
    return o;
}

int exec(const char* programname, arr<const char*>& args) {
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
}

wchar_t* env(const char* var) {
    // TODO ENCODING - We're assuming this is a UTF-8 string
    char* str_ch = getenv(var);
}

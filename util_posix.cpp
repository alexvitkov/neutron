#include "util.h"

const char PATH_SEPARATOR = ';';

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

const char* env(const char* var) {
    return getenv(var);
}

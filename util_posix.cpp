#include "util.h"
#include "linker.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <sstream>

Red red;
Dim dim;
ResetStyle resetstyle;

const char PATH_SEPARATOR = ':';

void init_utils() {
    arr<wchar_t*> path = read_path();
    for (wchar_t* entry : path) {

        arr<std::wstring> files;

        if (util_read_dir(entry, files, true)) {
            for (std::wstring& f : files) {
                if (f == L"ld") {
                    std::wostringstream s;
                    s << entry << "/ld";
                    has_gnu_ld = true;
                    gnu_ld.path = s.str();
                }
                if (f == L"ld.lld") {
                    std::wostringstream s;
                    s << entry << "/ld.lld";
                    has_lld = true;
                    lld.path = s.str();
                }
            }
        }
    }
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

int exec(std::wstring& programname, arr<std::wstring>& args) {
// int exec(const char* programname, arr<const char*>& args) {
    int child_pid = fork();
    if (child_pid) {
        int status;
        waitpid(child_pid, &status, 0);
        return WEXITSTATUS(status);
    }
    else {
        // TODO ENCODING wstring -> utf8
        char programname_c[1000];

        long i;
        const wchar_t* cs = programname.c_str();
        for (i = 0; cs[i]; i++)
            programname_c[i] = cs[i];
        programname_c[i] = 0;

        arr<char*> args_c;

        for (std::wstring& arg : args) {
            const wchar_t* cstr = arg.c_str();
            long len = wcslen(cstr);

            char* c_str = (char*)malloc(len + 1);

            for (i = 0; cstr[i]; i++)
                c_str[i] = cstr[i];
            c_str[i] = 0;

            args_c.push(c_str);
        }

        args_c.push(0);
        execvp(programname_c, args_c.buffer);

        std::wcout << "execvp failed: " << errno << "\n";
        assert(!"exec failed");
    }
}

wchar_t* env(const char* var) {
    // TODO ENCODING
    char* str_ch = getenv(var);
    long len = strlen(str_ch);

    wchar_t* str = (wchar_t*)malloc(sizeof(wchar_t) * (len + 1));
    long i;
    for (i = 0; i < len; i++) {
        str[i] = str_ch[i];
    }
    str[i] = 0;

    return str;
}

bool util_read_dir(const wchar_t* dirname, arr<std::wstring>& out, bool only_executable, const wchar_t* match) {

    char dirname_c[1000];
    int i;
    for (i = 0; dirname[i]; i++) {
        if (dirname[i] > 127)
            assert(!"Unknown encoding");
        dirname_c[i] = dirname[i];
    }
    dirname_c[i] = 0;
    
    if (match) {
        assert(!"match is only supported on Windows");
    }

    DIR *dir = opendir(dirname_c);
    if (!dir)
        return false;

    dirent* e;
    while ((e = readdir(dir))) {
        long length = strlen(e->d_name);

        if (!strcmp(".", e->d_name) || !strcmp("..", e->d_name))
            continue;

        // TODO ENCODING
        std::wstring fnamew(e->d_name, e->d_name + length);
        std::wstring cool = fnamew;
        out.push(cool);
    }

    closedir(dir);
    return true;
}

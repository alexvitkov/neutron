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
    arr<std::wstring> path = read_path();
    for (std::wstring& entry : path) {

        arr<std::wstring> files;

        if (util_read_dir(entry, files, true)) {
            for (std::wstring& f : files) {
                if (f == L"ld") {
                    has_gnu_ld = true;
                    gnu_ld.path = entry + L"/ld";
                }
                if (f == L"ld.lld") {
                    has_lld = true;
                    gnu_ld.path = entry + L"/ld.lld";
                }
            }
        }
    }
}


std::wostream& operator<<(std::wostream& o, Red& r) {
    o << "\e[0m";
    o << "\e[31;1m";
    return o;
}

std::wostream& operator<<(std::wostream& o, ResetStyle& r) {
    o << "\e[0m";
    return o;
}

std::wostream& operator<<(std::wostream& o, Dim& r) {
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

bool env(const char* var, std::wstring& out) {
    // ENCODING - we're assuming the result of getenv is either UTF-8 or ASCII
    char* str_ch = getenv(var);
    MUST(str_ch);

    out = utf8_to_wstring(str_ch);
    return true;
}

bool util_read_dir(std::wstring& dirname, arr<std::wstring>& out, bool only_executable, const wchar_t* match) {
    std::string dirname_utf8 = wstring_to_utf8(dirname);
    
    if (match) {
        assert(!"match is only supported on Windows");
    }

    DIR *dir = opendir(dirname_utf8.c_str());
    MUST(dir);

    dirent* e;
    while ((e = readdir(dir))) {
        long length = strlen(e->d_name);

        if (!strcmp(".", e->d_name) || !strcmp("..", e->d_name))
            continue;

        // ENCODING - we're assuming d_name is either UTF-8 or ASCII
        out.push(utf8_to_wstring(e->d_name));
    }

    closedir(dir);
    return true;
}

#include "common.h"
#include "util.h"
#include "cmdargs.h"
#include <dirent.h>


bool link_nix(const char* object_filename) {
    bool has_ld = false;
    bool has_lld = false;
    
    const char* path = env("PATH");

    arr<const char*> path_entries;

    int pathl = strlen(path);
    int start = 0;

    for (int i = 0; i < pathl; i++) {
        if (path[i] == ':') {
            int length = i - start;
            char* buf = (char*)malloc(length + 1);
            memcpy(buf,  path + start, length);
            path_entries.push(buf);

            start = i + 1;
        }
    }

    int length = pathl - start;
    char* buf = (char*)malloc(length + 1);
    memcpy(buf,  path + start, length);
    path_entries.push(buf);

    for (const char* dirname : path_entries) {
        DIR* dir;
        dirent *file;

        dir = opendir(dirname);
        if (!dir)
            continue;

        while ((file = readdir(dir)) != 0) {
            const char* fname = file->d_name;
            if (!strcmp(fname, "ld")) {
                has_ld = true;
            } else if (!strcmp(fname, "ld.lld")) {
                has_lld = true;
            }
        }
    }

    const char* linker;
    if (has_lld) {
        linker = "ld.lld";
    }
    else if (has_ld) {
        linker = "ld";
    }
    else {
        fprintf(stderr, "%s\n", "no linker found in path. searched for 'ld.lld' and 'ld'");
        return false;
    }


    arr<const char*> linker_args = { linker, object_filename, "-o", output_file, "-lc", "-L", "/lib", "--dynamic-linker=/lib/ld-linux-x86-64.so.2" };

    exec(linker, linker_args);

    return true;
}

void link(const char* object_filename) {
    if (target == TARGET_WINDOWS) {
        assert(!"LInking is not implemented on windows");
    } else {
        link_nix(object_filename);
    }

    remove(object_filename);
}


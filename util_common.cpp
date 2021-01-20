#include "util.h"


arr<wchar_t*> read_path() {
    wchar_t* path = env(L"PATH");

    arr<wchar_t*> path_entries;

    u64 start = 0;
    for (u64 i = 0 ;; i++) {
        if (path[i] == PATH_SEPARATOR || path[i] == '\0') {
            u64 length = i - start;
            wchar_t* buf = (wchar_t*)malloc(sizeof(wchar_t) * (length + 1));
            wcsncpy(buf, path + start, length);
            buf[length] = 0;

            path_entries.push(buf);

            if (path[i] == '\0')
                break;
            start = i + 1;
        }
    }

    free(path);

    return path_entries;
}
#include "util.h"

arr<std::wstring> read_path() {
    std::wstring path;

    if (!env("PATH", path))
        assert(!"ALALLALA");

    arr<std::wstring> path_entries;

    u64 start = 0;
    for (u64 i = 0 ;; i++) {
        if (path[i] == PATH_SEPARATOR || path[i] == '\0') {
            u64 length = i - start;
            if (length > 0) 
                path_entries.push(path.substr(start, length));

            if (path[i] == '\0')
                break;
            start = i + 1;
        }
    }

    return path_entries;
}

std::wstring utf8_to_wstring (const char* utf8_str) {
    // TODO those are deprecated in C++17
    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
    return myconv.from_bytes(utf8_str);
}

std::string wstring_to_utf8 (const std::wstring& str) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
    return myconv.to_bytes(str);
}

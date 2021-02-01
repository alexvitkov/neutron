#include "context.h"
#include "util.h"

void run_parse_tests() {
    arr<std::wstring> files;
    std::wstring dir = L"./test/must_parse";
    if (!util_read_dir(dir, files))
        wcout << "Could not find ./test/must_parse/. Some tests will not be run!\n";

    for (auto& f : files) {
        std::wstring path = dir + L"/" + f;
        std::wcout << "Test case " << path << "... ";

        GlobalContext global;

        u32 sf_id;
        if (!add_source(path, &sf_id)) {
            wcout << red << "COULDN'T TO READ FILE\n" << resetstyle;
            continue;
        }

        if (parse_source_file(global, sources[sf_id])) {
            std::wcout << "OK.\n";
        }
        else {
            std::wcout << red << "FAIL:\n" << resetstyle;
            for (auto& err : global.errors)
                print_err(global, err);
            std::wcout << "--------------------\n";
        }

    }
}



int main() {
    run_parse_tests();
    return 0;
}

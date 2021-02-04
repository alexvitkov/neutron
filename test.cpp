#include "context.h"
#include "util.h"
#include "ast.h"

void run_parse_tests() {
    arr<std::wstring> files;
    std::wstring dir = L"./test/must_parse";
    if (!util_read_dir(dir, files)) {
        wcout << red << "Could not find ./test/must_parse -  Some tests will not be run!\n" << resetstyle;
        return;
    }

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
            std::wcout << red << "PARSE ERROR:\n" << resetstyle;
            for (auto& err : global.errors)
                print_err(global, err);
            std::wcout << "--------------------\n";
        }

    }
}

void run_tree_compare_tests() {
    arr<std::wstring> files;
    std::wstring dir = L"./test/tree_compare";
    if (!util_read_dir(dir, files)) {
        wcout << red << "Could not find ./test/tree_compare - Some tests will not be run!\n" << resetstyle;
        return;
    }

    for (auto& f : files) {
        std::wstring path = dir + L"/" + f;
        std::wcout << "Test case " << path << "... ";

        GlobalContext global;

        u32 sf_id;
        if (!add_source(path, &sf_id)) {
            wcout << red << "COULDN'T TO READ FILE\n" << resetstyle;
            continue;
        }

        if (!parse_source_file(global, sources[sf_id])) {
            std::wcout << red << "PARSE ERROR:\n" << resetstyle;
            for (auto& err : global.errors)
                print_err(global, err);
            std::wcout << "--------------------\n";
            continue;
        }

        map<char*, arr<AST_Node*>> groups;

        for (auto& decl : global.declarations) {
            if (!decl.key.name)
                continue;

            // buffer is the same as the declaration's name
            // but without the last character
            // TODO MEMORY LEAK - we're leaking the buffer of course
            size_t length = strlen(decl.key.name) - 1;
            char* buffer = (char*)malloc(length);
            memcpy(buffer, decl.key.name, length);
            buffer[length] = 0;

            groups[buffer].push(decl.value);
        }

        char *fail = nullptr;

        for (auto& group : groups) {
            for (u32 i = 1; i < group.value.size; i++) {
                if (!postparse_tree_compare(group.value[0], group.value[i])) {
                    fail = group.key;
                    goto DONE;
                }
            }
        }

DONE:
        if (!fail) {
            wcout << "OK.\n";
        } else {
            wcout << red << "FAIL at " << fail << "*\n" << resetstyle;
        }

    }
}


int main() {
    run_parse_tests();
    run_tree_compare_tests();
    return 0;
}

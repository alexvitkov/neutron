#include "common.h"
#include "util.h"
#include "cmdargs.h"
#include "linker.h"

#include <sstream>

bool has_msvc_linker;
MSVCLinker msvc_linker;

/*
bool link_nix(const char* object_filename) {
    bool has_ld = false;
    bool has_lld = false;
    
    // const wchar_* path = env(L"PATH");

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

    // exec(linker, linker_args);

    return true;
}

bool link_windows(const char* object_filename) {
    std::wstring linker_path = find_linker();

    // arg<std::wstring> linker_args = 

    return false;
}



void link(const char* object_filename) {
    if (target == TARGET_WINDOWS) {
        link_windows(object_filename);
    } else {
        link_nix(object_filename);
    }

    remove(object_filename);
}
*/

bool link(MSVCLinker& linker, const std::wstring& object_name, const std::wstring& output_path) {

    std::wostringstream out_arg, lib_arg1, lib_arg2, lib_arg3, lib_arg4;
    
    out_arg << "/OUT:" << output_path;
    
    lib_arg1 << "/LIBPATH:" << linker.msvc_base_path << "\\lib\\x64";
    lib_arg2 << "/LIBPATH:" << linker.msvc_base_path << "\\ATLMFC\\lib\\x64";
    lib_arg3 << "/LIBPATH:" << linker.windows_sdk_base_path << "\\lib\\" << linker.windows_sdk_version << "\\um\\x64";
    lib_arg4 << "/LIBPATH:" << linker.windows_sdk_base_path << "\\lib\\" << linker.windows_sdk_version << "\\ucrt\\x64";
    
    arr<std::wstring> args = { 
        linker.link_exe_path, 
        lib_arg1.str(), lib_arg2.str(), lib_arg3.str(), lib_arg4.str(), 
        out_arg.str(),
        object_name,
        L"LIBCMT.lib" 
    };

    int result = exec(linker.link_exe_path, args);
    return !result;

}
#include "common.h"
#include "util.h"
#include "cmdargs.h"
#include "linker.h"

#include <sstream>

bool has_msvc_linker = false;
MSVCLinker msvc_linker;

bool has_lld = false;
LDLinker lld;

bool has_gnu_ld = false;
LDLinker gnu_ld;

bool link(LDLinker& linker, const std::wstring& object_name, const std::wstring& output_path) {
    arr<std::wstring> linker_command_line = { 
        linker.path, 
        object_name, 
        L"-o", output_path, 
        L"-lc", 
        L"-L", L"/lib", 
        L"--dynamic-linker=/lib/ld-linux-x86-64.so.2" 
    };

    int result = exec(linker.path, linker_command_line);
    return !result;
}


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

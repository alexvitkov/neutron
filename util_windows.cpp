#define WIN32_LEAN_AND_MEAN
#define _UNICODE
#define UNICODE
#include <Windows.h>

#include <comdef.h>
#include <sstream>
#include <algorithm>

#include "util.h"
#include "linker.h"

HANDLE console;
CONSOLE_SCREEN_BUFFER_INFO csbi;

const char PATH_SEPARATOR = ';';

Red red;
Dim dim;
ResetStyle resetstyle;

#include <Setup.Configuration.h>
_COM_SMARTPTR_TYPEDEF(ISetupConfiguration, __uuidof(ISetupConfiguration));
_COM_SMARTPTR_TYPEDEF(IEnumSetupInstances, __uuidof(IEnumSetupInstances));
_COM_SMARTPTR_TYPEDEF(ISetupInstance, __uuidof(ISetupInstance));
_COM_SMARTPTR_TYPEDEF(ISetupInstanceCatalog, __uuidof(ISetupInstanceCatalog));
_COM_SMARTPTR_TYPEDEF(ISetupPropertyStore, __uuidof(ISetupPropertyStore));

struct VisualStudioInstall {
    VARIANT version;
    BSTR path;
};
arr<VisualStudioInstall> visual_studio_installs;



bool find_msvc_linker() {
    ISetupConfigurationPtr setupCfg;
    IEnumSetupInstancesPtr enumInstances;

    setupCfg.CreateInstance(__uuidof(SetupConfiguration));
    setupCfg->EnumInstances(&enumInstances);

    while (true) {
        ISetupInstance* p = nullptr;
        unsigned long ul = 0;
        HRESULT hr = enumInstances->Next(1, &p, &ul);
        if (hr != S_OK)
            break;

        ISetupInstancePtr setupi(p, false);
        ISetupInstanceCatalogPtr instanceCatalog;
        ISetupPropertyStorePtr store;
        setupi->QueryInterface(&instanceCatalog);
        instanceCatalog->GetCatalogInfo(&store);

        VisualStudioInstall install;

        store->GetValue(L"productLineVersion", &install.version);
        setupi->GetInstallationPath(&install.path);

        visual_studio_installs.push(install);
    }

    if (visual_studio_installs.size == 0) {
        // TODO ERROR
        fprintf(stderr, "WARNING: Could not find Visual Studio. Cannot link a .exe file.\n");
        has_msvc_linker = false;
        CoUninitialize();
        return false;
    }

    msvc_linker.visual_studio_base_path = visual_studio_installs[0].path;




    // Find MSVC inside the Visual Studio directory
    std::wstring msvc_base_path = msvc_linker.visual_studio_base_path + L"\\VC\\Tools\\MSVC";

    arr<std::wstring> msvc_versions;
    if (!util_read_dir(msvc_base_path, msvc_versions)) {
        fprintf(stderr, "WARNING: Could not MSVC inside the Visual Studio directory. Cannot link a .exe file.\n");
        return false;
    }

    // The foldernames inside the MSVC subdirectory are the version names. Sort by them and pick the last one (latest version)
    std::sort(msvc_versions.begin(), msvc_versions.end());

    msvc_linker.msvc_version = msvc_versions.last();
    msvc_linker.msvc_base_path = msvc_base_path + L"\\" + msvc_linker.msvc_version;

    // TODO we're just assuming the file exists
    bool host_x64 = true, target_x64 = true;
    msvc_linker.link_exe_path 
        = msvc_linker.msvc_base_path + L"\\bin\\Host" 
        + (host_x64 ? L"x64\\" : L"x86\\") 
        + (target_x64 ? L"x64\\link.exe" : L"x86\\link.exe");    

    DWORD type;
    LSTATUS status;
    wchar_t buffer[1000];
    DWORD size_in_bytes = 1000 * sizeof(wchar_t);
    status = RegGetValueW(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\WOW6432Node\\Microsoft\\Microsoft SDKs\\Windows\\v10.0",
        L"InstallationFolder",
        RRF_RT_REG_SZ,
        &type,
        buffer,
        &size_in_bytes);

    if (status != ERROR_SUCCESS) {
        // TODO ERROR
        fprintf(stderr, "WARNING: Could not find a Windows 10 SDK. Cannot link a .exe file\n");
        return false;
    }

    msvc_linker.windows_sdk_base_path = buffer;


    arr<std::wstring> windows_kit_versions;
    std::wstring windows_kit_path = msvc_linker.windows_sdk_base_path + L"\\bin";

    if (!util_read_dir(windows_kit_path, windows_kit_versions, false, L"10.*")) {
        // TODO ERROR
        fprintf(stderr, "WARNING: Could not find a Windows 10 SDK. Cannot link a .exe file\n");
        return false;
    }
    
    // The foldernames inside the MSVC subdirectory are the version names. Sort by them and pick the last one (latest version)
    std::sort(windows_kit_versions.begin(), windows_kit_versions.end());

    msvc_linker.windows_sdk_version = windows_kit_versions.last();

    has_msvc_linker = true;
    return true;
}


void init_utils() {
    CoInitialize(nullptr);
    console = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(console, &csbi);

    find_msvc_linker();
    CoUninitialize();
}


std::ostream& operator<<(std::ostream& o, Red& r) {
    SetConsoleTextAttribute(console, 12 | csbi.wAttributes & 0x00F0);
    return o;
}

std::ostream& operator<<(std::ostream& o, ResetStyle& r) {
    SetConsoleTextAttribute(console, csbi.wAttributes);
    return o;
}

std::ostream& operator<<(std::ostream& o, Dim& r) {
    SetConsoleTextAttribute(console, 8 | csbi.wAttributes & 0x00F0);
    return o;
}

int exec(std::wstring& programname, arr<std::wstring>& args) {
    // TODO this is stupid
    std::wostringstream cmdline_builder;
    for (auto& arg : args) {
        cmdline_builder << L'"' << arg << "\" ";
    }

    std::wstring cmdline_str = cmdline_builder.str();
    const wchar_t* cmdline_const = cmdline_str.c_str();
    wchar_t* cmdl = (wchar_t*)malloc(sizeof (wchar_t) * (wcslen(cmdline_const) + 1));
    wcscpy(cmdl, cmdline_const);
    

    wprintf(L"%s\n%s\n", programname.c_str(), cmdl);

    PROCESS_INFORMATION pi;
    STARTUPINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessW(NULL,
        cmdl,
        NULL, 
        NULL, 
        FALSE,
        0,
        NULL,
        NULL,
        &si,
        &pi))
    {
        // TODO ERROR
        fprintf(stderr, "exec failed: GetLastError = %d\n", GetLastError());
        return false;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    return exitCode;
}

bool env(const char* var, std::wstring& out) {
    // TODO BUFFER long variables are problematic
    const int buf_size = 2048;
    wchar_t buf[buf_size];

    std::wstring var_w = utf8_to_wstring(var);
    
    DWORD length = GetEnvironmentVariableW(var_w.c_str(), buf, buf_size);

    if (length) {
        buf[length] = 0;
        out = buf;
        return true;
    }
    else {
        return false;
    }
}

bool util_read_dir(std::wstring& dirname, arr<std::wstring>& out, bool only_executable, const wchar_t* match) {
    WIN32_FIND_DATAW data;
    
    // TODO BUFFER
    std::wostringstream sPath;
    sPath << dirname << L"\\";

    if (match) {
        sPath << match;
    } else if (only_executable) {
        sPath << "*.exe";
    } else {
        sPath << "*.*";
    }
    HANDLE f = FindFirstFileW(sPath.str().c_str(), &data);

    if (f == INVALID_HANDLE_VALUE) {
        return false;
    }

    do {
        std::wstring filename = data.cFileName;
        if (filename != L"." && filename != L"..")
            out.push(std::move(filename));
    } while (FindNextFileW(f, &data));

    FindClose(f);
    return GetLastError() == ERROR_NO_MORE_FILES;
}
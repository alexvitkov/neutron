#ifndef LINKER_H
#define LINKER_H

#include <string>


// MSVC linker is either the link.exe that comes with Visual Studio
// or clang's lld-link, which emulates its command line interface
struct MSVCLinker {
	std::wstring 
		visual_studio_base_path,
		
		msvc_base_path,
		msvc_version,
		link_exe_path,

		windows_sdk_base_path,
		windows_sdk_version;
};

bool link(MSVCLinker& linker, const std::wstring& object_name, const std::wstring& output_path) ;


extern bool has_msvc_linker;
extern MSVCLinker msvc_linker;

#endif // guard
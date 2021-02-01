#ifndef UTIL_H
#define UTIL_H

#include "ds.h"
#include <iostream>
#include <wchar.h>
#include <codecvt>
#include <string>
#include <locale>

#ifdef _MSC_VER
#   define UNREACHABLE __assume(false)
#else
#   define UNREACHABLE __builtin_unreachable()
#endif

#define DIE(msg) { assert(!(msg)); UNREACHABLE; }

#define NOT_IMPLEMENTED(...) assert(!(__VA_ARGS__ " not implemented"));

struct Red { char _; };
struct ResetStyle { char _; };
struct Dim { char _; };

extern Red red;
extern ResetStyle resetstyle;
extern Dim dim;

extern const char PATH_SEPARATOR;

std::ostream& operator<<(std::ostream&, Red& r);
std::ostream& operator<<(std::ostream&, ResetStyle& r);
std::ostream& operator<<(std::ostream&, Dim& r);

void init_utils();
int exec(std::wstring& programname, arr<std::wstring>& args);

bool env(const char* var, std::wstring& out);

arr<std::wstring> read_path();

bool util_read_dir(std::wstring& dirname, arr<std::wstring>& out, bool only_executable = false, const wchar_t* match = nullptr);

// https://stackoverflow.com/questions/4358870/convert-wstring-to-string-encoded-in-utf-8
std::wstring utf8_to_wstring (const char* utf8_str);
std::string wstring_to_utf8 (const std::wstring& str);


#endif // guard

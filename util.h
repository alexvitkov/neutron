#ifndef UTIL_H
#define UTIL_H

#include "ds.h"
#include <iostream>
#include <wchar.h>

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
int exec(const char* programname, arr<const char*>& args);

wchar_t* env(const char* var);

// You should free all the entries of read_path when done
arr<wchar_t*> read_path();

bool util_read_dir(const wchar_t* dirname, arr<wchar_t*>& out);

#endif // guard

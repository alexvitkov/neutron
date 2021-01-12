#ifndef UTIL_H
#define UTIL_H

#include "ds.h"
#include <iostream>

struct Red { char _; };
struct ResetStyle { char _; };
struct Dim { char _; };

extern Red red;
extern ResetStyle resetstyle;
extern Dim dim;

std::ostream& operator<<(std::ostream&, Red& r);
std::ostream& operator<<(std::ostream&, ResetStyle& r);
std::ostream& operator<<(std::ostream&, Dim& r);

void init_utils();
int exec(const char* programname, arr<const char*>& args);

const char* env(const char* var);

#endif // guard

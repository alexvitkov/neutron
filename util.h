#ifndef UTIL_H
#define UTIL_H

#include <iostream>

struct Red { 
	char _;
};

struct ResetStyle { 
	char _; 
};

struct Dim {
	char _;
};

extern Red red;
extern ResetStyle resetstyle;
extern Dim dim;

void init_utils();

std::ostream& operator<<(std::ostream&, Red& r);
std::ostream& operator<<(std::ostream&, ResetStyle& r);
std::ostream& operator<<(std::ostream&, Dim& r);

#endif // guard
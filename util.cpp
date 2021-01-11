#include "util.h"

#ifdef _WINDOWS
#include "Windows.h"

HANDLE console;
CONSOLE_SCREEN_BUFFER_INFO csbi;
#endif

Red red;
ResetStyle resetstyle;
Dim dim;

void init_utils() {
#ifdef _WINDOWS
	console = GetStdHandle(STD_OUTPUT_HANDLE);
	GetConsoleScreenBufferInfo(console, &csbi);
#endif
}


std::ostream& operator<<(std::ostream& o, Red& r) {
#ifdef _WINDOWS
	SetConsoleTextAttribute(console, 12 | csbi.wAttributes & 0x00F0);
#else
	o << "\e[31;1m";
#endif
	return o;
}

std::ostream& operator<<(std::ostream& o, ResetStyle& r) {
#ifdef _WINDOWS
	SetConsoleTextAttribute(console, csbi.wAttributes);
#else
	o << "\e[0m";
#endif
	return o;
}

std::ostream& operator<<(std::ostream& o, Dim& r) {
#ifdef _WINDOWS
	SetConsoleTextAttribute(console, 8 | csbi.wAttributes & 0x00F0);
#else
	o << "\e[2m";
#endif
	return o;
}
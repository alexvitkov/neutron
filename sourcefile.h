#ifndef SOURCEFILE_H
#define SOURCEFILE_H

#include "parser.h"

#include <stdio.h>
#include <stdlib.h>

struct SourceFile {
	size_t id;
	long length;
	char *buffer;
	std::vector<Token> tokens;
};

extern std::vector<SourceFile> sources;

int add_source(const char* filename);

#endif // guard

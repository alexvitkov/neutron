#ifndef SOURCEFILE_H
#define SOURCEFILE_H

#include "common.h"
#include "ds.h"

struct SourceFile {
	size_t id;
	long length;
	char *buffer;
	arr<Token> tokens;
};

extern arr<SourceFile> sources;

int add_source(const char* filename);

#endif // guard

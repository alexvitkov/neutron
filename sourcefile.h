#ifndef SOURCEFILE_H
#define SOURCEFILE_H

#include "common.h"
#include "ds.h"

struct SourceFile {
	size_t id;
	long length;
	char *buffer;
    const char* filename;
	arr<Token> tokens;

    // line_start[100] is the character at which the 100th line starts
    arr<u32> line_start;
};

extern arr<SourceFile> sources;

int add_source(const char* filename);

#endif // guard

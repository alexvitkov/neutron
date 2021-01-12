#ifndef CMDARGS_H
#define CMDARGS_H

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

enum OutputType {
    OUTPUT_OBJECT_FILE,
    OUTPUT_LINKED_EXECUTABLE
};

enum Target {
    TARGET_WINDOWS,
    TARGET_UNIX,
};

extern arr<SourceFile> sources;
extern OutputType output_type;
extern const char* output_file;
extern Target target;
extern bool debug_output;

bool parse_args(int argc, const char** argv);

#endif // guard

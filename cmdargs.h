#ifndef CMDARGS_H
#define CMDARGS_H

#include "common.h"
#include "ds.h"

struct SourceFile {
	u32 id;

	u64 length;

	char *buffer;
    std::wstring filename;

    // line_start[100] is the character at which the 100th line starts
    arr<u64> line_start;

	arr<SmallToken> _tokens;
    arr<LocationInFile> _token_locations;

    Token pushToken(SmallToken st, LocationInFile loc);
    Token getToken(u64 tok_id);
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
extern bool print_llvm, print_tir, print_ast, exec_main, debug_jobs;

bool add_source(std::wstring& filename, u32* out);
bool parse_args(int argc, const char** argv);

#endif // guard

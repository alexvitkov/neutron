#include "sourcefile.h"

arr<SourceFile> sources;

int add_source(const char* filename) {
	FILE* f;
	SourceFile sf;

	if (!(f = fopen(filename, "rb"))) {
		return -1;
	}

	fseek(f, 0, SEEK_END);
	sf.length = ftell(f);
	rewind(f);

	if (!(sf.buffer = (char*)malloc(sf.length))) {
		fclose(f);
		return -1;
	}
	if (fread(sf.buffer, 1, sf.length, f) < sf.length) {
		fclose(f);
		return -1;
	}
	fclose(f);
	
	sf.id = sources.size;
    sf.filename = filename;

	sources.push(std::move(sf));
	return sf.id;
}

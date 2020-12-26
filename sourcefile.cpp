#include "sourcefile.h"

std::vector<SourceFile> sources;

int add_source(const char* filename) {
	FILE* f;
	SourceFile sf;

	if (!(f = fopen(filename, "r"))) {
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
	
	sf.id = sources.size();

	sources.push_back(sf);
	return sf.id;
}

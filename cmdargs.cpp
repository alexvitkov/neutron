#include "cmdargs.h"

bool debug_output = false;
const char* output_file = nullptr;
OutputType output_type;
arr<SourceFile> sources;

#ifdef _WINDOWS
Target target = TARGET_WINDOWS;
#else
Target target = TARGET_UNIX;
#endif


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


bool parse_args(int argc, const char** argv) {
    // Once -- has been passed, we no longer read command line switches, just files
    bool done_with_switches = false; 

    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];

        if (!done_with_switches && a[0] == '-') {
            if (a[1] == '-') {
                // User has passed -- ; everything after it is read as input file
                if (!a[2]) {
                    done_with_switches = true;
                    continue;
                }

                // User has passed --foo, check what foo is
                const char* argname = a + 2;
                if (!strcmp(argname, "output")) {
                    if (i >= argc - 1) {
                        fprintf(stderr, "missing --output argument\n");
                        return false;
                    }
                    else {
                        output_file = argv[++i];
                        continue;
                    }
                }
            }
            else if (strlen(a) == 2) {
                switch (a[1]) {
                    case 'o': {
                        if (i >= argc - 1) {
                            fprintf(stderr, "missing -o argument\n");
                            return false;
                        }
                        else {
                            output_file = argv[++i];
                            continue;
                        }
                        break;
                    }
                    case 'd': {
                        debug_output = true;
                        continue;
                    }
                }
            }
        } 

        // If the arg hasn't been parsed as a command line flag, it's a input file
        if (add_source(a) < 0) {
            fprintf(stderr, "failed to read source '%s'\n", a);
            return false;
        }
    }


    // If there's no output file we're assuming the user wants an executable with the default name
    if (!output_file) {
        output_file = TARGET_WINDOWS ? "main.exe" : "a.out";
        output_type = OUTPUT_LINKED_EXECUTABLE;
    } else {

        // Find out the filename extension
        // If there isn't one, extension is just an empty string
        int fnamelen = strlen(output_file);
        int lastdot = fnamelen - 1;
        for (int i = 0; i < fnamelen; i++) {
            if (output_file[i] == '.')
                lastdot = i;
        }
        const char* extension = output_file + lastdot + 1;

        if (!strcmp(extension, "o")) {
            output_type = OUTPUT_OBJECT_FILE;
        } else  {
            if (TARGET_WINDOWS) {
                // For windows we only accept .o and .exe files
                if (!strcmp(extension, "exe")) {
                    output_type = OUTPUT_LINKED_EXECUTABLE;
                } else {
                    fprintf(stderr, "%s\n", "output file must have .exe or .o extension");
                    exit(1);
                }
            } else {
                // For *nix everything that's not a .o file we treat as an executable
                output_type = OUTPUT_LINKED_EXECUTABLE;
            }
        }
    }

    return true;
}

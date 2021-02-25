#include "util.h"
#include "cmdargs.h"

bool print_llvm, print_tir, print_ast, exec_main, debug_jobs;

const char* output_file = nullptr;
OutputType output_type;
arr<SourceFile> sources;

#ifdef _WINDOWS
Target target = TARGET_WINDOWS;
#else
Target target = TARGET_UNIX;
#endif


bool add_source(std::wstring& filename, u32* out) {
	FILE* f;
	SourceFile sf;

    // TODO ENCODING
    // this should use native apis
    std::string filename_utf8 = wstring_to_utf8(filename);

    // TODO ERROR
	if (!(f = fopen(filename_utf8.c_str(), "rb"))) {
		return false;
	}

	fseek(f, 0, SEEK_END);
	sf.length = ftell(f);
	rewind(f);

    // TODO ERROR
	if (!(sf.buffer = (char*)malloc(sf.length))) {
		fclose(f);
		return false;
	}
	if (fread(sf.buffer, 1, sf.length, f) < sf.length) {
		fclose(f);
		return false;
	}
	fclose(f);
	
	sf.id = sources.size;
    sf.filename = filename;

	sources.push(std::move(sf));
    if (out)
        *out = sf.id;
	return true;
}


bool parse_args(int argc, const char** argv) {
    // Once "--" has been passed, we no longer read command line switches, just files
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
                if (!strcmp(argname, "exec_main")) {
                    exec_main = true;
                }
            }
            else {
                for (const char *flag = a + 1; *flag; flag++) {
                    switch (*flag) {
                        case 'o': {
                            if (i >= argc - 1) {
                                fprintf(stderr, "missing -o argument\n");
                                return false;
                            }
                            else {
                                output_file = argv[++i];
                                break;
                            }
                            break;
                        }
                        case 'l': {
                            print_llvm = true;
                            break;
                        }
                        case 't': {
                            print_tir = true;
                            break;
                        }
                        case 'a': {
                            print_ast = true;
                            break;
                        }
                        case 'j': {
                            debug_jobs = true;
                            break;
                        }
                        case 'e': {
                            exec_main = true;
                            break;
                        }
                        default:
                            assert(!"UNKNOWN FLAG AAAAAAA");
                    }
                }
                continue;
            }
        } 

        // If the arg hasn't been parsed as a command line flag, it's a input file
        // TODO ENCODING - this is wrong on windows
        std::wstring filename = utf8_to_wstring(a);
        if (!add_source(filename, nullptr)) {
            fprintf(stderr, "failed to read source '%s'\n", a);
            return false;
        }
    }


    // If there's no output file we're assuming the user wants an executable with the default name
    if (!output_file) {
        output_file = target == TARGET_WINDOWS ? "main.exe" : "a.out";
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


Token SourceFile::getToken(u64 tok_id) {
    Token tok;
    *(SmallToken*)&tok = _tokens[tok_id];

    tok.file_id = id;
    tok.id = tok_id;
    tok.loc = _token_locations[tok_id];

    return tok;
}

Token SourceFile::pushToken(SmallToken st, LocationInFile loc) {
    _tokens.push(st);
    _token_locations.push(loc);
    return getToken(_tokens.size - 1);
}

#include "sourcefile.h"
#include "parser.h"
#include "typer.h"
#include "backends/bytecode/bytecode.h"

Context global { .global = &global };

void exit_with_error() {
	for (auto& err : global.errors) {
		err.print();
	}
    exit(1);
}

int main(int argc, const char** argv) {
	bool dd = false;
	for (int i = 1; i < argc; i++) {
		const char* a = argv[i];

		if (!dd && a[0] == '-') {
			if (a[1] == '-') {
				dd = true;
				continue;
			}
		}
		else {
			if (add_source(a) < 0) {
				printf("failed to read source '%s'\n", a);
                exit(1);
			}
		}
	}
	if (sources.size() == 0) {
		printf("no input files\n");
        exit(1);
	}

	if (!parse_all_files(global)) {
        printf("parser failed\n");
        exit_with_error();
    }

    if (!typecheck_all(global)) {
        printf("type checker failed\n");
        exit_with_error();
    }

    void* code = malloc(10 * 1024 * 1024);
    void* stac = malloc(10 * 1024);

    void* end = bytecode_compile(global, code, stac);

    bytecode_disassemble((u8*)code, (u8*)end);

    return 0;
}

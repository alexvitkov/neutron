#include "sourcefile.h"
#include "parser.h"
#include "typer.h"

void cool() {
	/*
    int* five_val = (int*)malloc(sizeof(int));
    *five_val = 5;
    value FIVE = { NT_VALUE, VAL_INTEGER, five_val };


    uint64_t reg[NREG] = { 0 };
    uint8_t mem[1024] = { 0 };

    reg[SP] = 800;

    uint8_t *p = mem;
    emitv2r(&p, OP_MOV, 4, 0);
    emitr2r(&p, OP_MOV, 1, 0);
    emitv2r(&p, OP_MULS, 2, 0);
    emitr2r(&p, OP_SUB, 0, 1);

    interpret(reg, mem);

    printreg(reg);
	*/
}

void die() {
	exit(1);
}

Context global { .global = &global };

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
				die();
			}
		}
	}
	if (sources.size() == 0) {
		printf("no input files\n");
		die();
	}

	if (!parse_all_files(global)) {
        printf("parser failed\n");
        goto PrintErr;
    }

    if (!typecheck_all(global)) {
        printf("type checker failed\n");
        goto PrintErr;
    }

    for (const auto& x : global.defines) {
        print(std::cout, x.second, true);
        std::cout << "\n";
    }


PrintErr:
	for (auto& err : global.errors) {
		err.print();
	    return 1;
	}

    return 0;
}
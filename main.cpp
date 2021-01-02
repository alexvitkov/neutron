#include "sourcefile.h"
#include "typer.h"
#include "ast.h"
#include "error.h"
#include "backend/bytecode/bytecode.h"

GlobalContext global;

void exit_with_error() {
	for (auto& err : global.errors) {
        print(global, err);
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
	if (sources.size == 0) {
		printf("no input files\n");
        exit(1);
	}

	if (!parse_all_files(global)) {
        exit_with_error();
    }
    
    for (const auto& decl : global.declarations_arr) {
        print(std::cout, decl.node, true);
        std::cout << '\n';
    }

    if (!typecheck_all(global)) {
        printf("type checker failed\n");
        exit_with_error();
    }

    BytecodeContext c(global);

    bytecode_commpile_all(c);
    bytecode_disassemble_all(c);

    // c.disassemble_all();

    // Find main address
//   u32 mainfn_addr;
//   if (c.fnaddr.find("main", &mainfn_addr)) {
//       u64 ret = interpret(c, mainfn_addr);
//       printf("main returned %lu\n", ret);
//   }
//   else {
//       printf("There is no main function.\n");
//   }

    return 0;
}

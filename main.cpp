#include "sourcefile.h"
#include "typer.h"
#include "ast.h"
#include "error.h"
#include "backend/tir/tir.h"
#include "backend/llvm/llvm.h"

GlobalContext global;

void exit_with_error() {
    printf("Error\n");
	for (auto& err : global.errors) {
        print_err(global, err);
	}
    exit(1);
}

void redd() {
    printf("\e[31;1m");
}

void resetstylee() {
    printf("\e[0m");
}

const char* output_file;

bool parse_args(int argc, const char** argv) {

    bool done_with_args = false; 

    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];

        if (!done_with_args && a[0] == '-') {
            if (a[1] == '-') {
                // User has passed -- ; everything after it is read as input file
                if (!a[2]) {
                    done_with_args = true;
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
                }
            }
        } 

        // If the arg hasn't been parsed as a command line flag, it's a input file
        if (add_source(a) < 0) {
            fprintf(stderr, "failed to read source '%s'\n", a);
            return false;
        }
    }

    return true;
}

int main(int argc, const char** argv) { 

    if (!parse_args(argc, argv)) {
        return 1;
    }

    if (sources.size == 0) {
        printf("no input files\n");
        exit(1);
    }

    if (!parse_all(global)) {
        exit_with_error();
    }

    if (!typecheck_all(global)) {
        printf("type checker failed\n");
        exit_with_error();
    }

    // Print the source code
    redd();
    printf("----------AST----------\n");
    resetstylee();

    for (const auto& decl : global.declarations_arr) {
        print(std::cout, decl.node, true);
        std::cout << '\n';
    }

    TIR_Context t_c { .global = global };
    t_c.compile_all();

    // Print the TIR
    redd();
    printf("\n----------TIR----------\n");
    resetstylee();

    for (auto& kvp : t_c.fns) {
        printf("%s:\n", kvp.key->name);
        TIR_Block* block = kvp.value->entry;
        for (auto& instr : block->instructions)
            std::cout << instr;
    }

    // Generate the 
    redd();
    printf("\n----------LLVM IR----------\n");
    resetstylee();
    fflush(stdout);

    LLVM_Context lllvmcon(t_c);
    lllvmcon.compile_all();

    lllvmcon.output_object();


    return 0;
}

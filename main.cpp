#include "typer.h"
#include "ast.h"
#include "cmdargs.h"
#include "error.h"
#include "util.h"
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

void link(const char* object_filename);

int main(int argc, const char** argv) {
    init_utils();

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

    if (debug_output) {
        std::cout << red << "--------- AST ---------\n" << resetstyle;

        for (const auto& decl : global.declarations_arr) {
            printf("%p\n", ((AST_Value*)decl.node)->type);

            print(std::cout, decl.node, true);
            std::cout << '\n';

        }
    }

    TIR_Context t_c { .global = global };
    t_c.compile_all();

    // Print the TIR
    if (debug_output) {
        std::cout << red << "\n--------- TIR ---------\n" << resetstyle;

        for (auto& kvp : t_c.fns) {
            printf("%s:\n", kvp.key->name);

            if (kvp.value->ast_fn->is_extern)  {
                printf("    extern\n");
            } else {
                TIR_Block* block = kvp.value->entry;
                for (auto& instr : block->instructions)
                    std::cout << instr;
            }
        }
    }

    if (debug_output) {
        std::cout << red << "\n------- LLVM IR -------\n" << resetstyle;
        // The LLVM IR is pritned by the compile_all function
    }
    fflush(stdout);

    LLVM_Context lllvmcon(t_c);
    lllvmcon.compile_all();

    const char* object_filename = lllvmcon.output_object();

    if (output_type == OUTPUT_LINKED_EXECUTABLE) {
        link(object_filename);
    }

    return 0;
}

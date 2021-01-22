#include "typer.h"
#include "ast.h"
#include "cmdargs.h"
#include "error.h"
#include "linker.h"
#include "util.h"
#include "backend/tir/tir.h"
#include "backend/llvm/llvm.h"

GlobalContext global;

void exit_with_error() {
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
        // printf("type checker failed with %d errors\n", global.errors.size);
        exit_with_error();
    }

    // Print the source code
    if (debug_output) {
        std::cout << red << "--------- AST ---------\n" << resetstyle;

        for (const auto& decl : global.declarations) {
            print(std::cout, decl.value, true);
            std::cout << '\n';

        }
    }

    TIR_Context t_c { .global = global };
    t_c.compile_all();

    // Print the TIR
    if (debug_output) {
        std::cout << red << "\n--------- TIR ---------\n" << resetstyle;

        for (auto& kvp : t_c.fns) {
            kvp.value->print();
        }
        std::cout.flush();
    }
    // return 0;

    if (debug_output) {
        std::cout << red << "\n------- LLVM IR -------\n" << resetstyle;
        // The LLVM IR is pritned by the compile_all function
    }
    fflush(stdout);

    T2L_Context lllvmcon(t_c);
    lllvmcon.compile_all();

    const char* object_filename = lllvmcon.output_object();

    if (output_type == OUTPUT_LINKED_EXECUTABLE) {
        // link(object_filename);
        if (has_msvc_linker) {
            std::string of = object_filename;
            std::wstring object_filename_w(of.begin(), of.end());

            std::string ouf = output_file;
            std::wstring output_filename_w(ouf.begin(), ouf.end());

            link(msvc_linker, object_filename_w, output_filename_w);
        }
    }

    return 0;
}

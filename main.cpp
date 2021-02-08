#include "backend/llvm/llvm.h" // include this first
#include "backend/tir/tir.h"
#include "typer.h"
#include "ast.h"
#include "cmdargs.h"
#include "error.h"
#include "linker.h"
#include "util.h"

AST_GlobalContext global;

void exit_with_error() {
	for (auto& err : global.errors) {
        print_err(global, err);
	}
    exit(1);
}

void link(const char* object_filename);

void main_exec_job_on_done(Job *job) {
    TIR_ExecutionJob *exec = (TIR_ExecutionJob*)job;
    wcout << red << "\n------ EXECUTION ------\n" << resetstyle;
    wcout << "main returned " << (i64)exec->next_retval << "\n";
}

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
        exit_with_error();
    }

    // Print the source code
    if (debug_output) {
        wcout << red << "--------- AST ---------\n" << resetstyle;
        for (const auto& decl : global.declarations) {
            print(wcout, decl.value, true);
            wcout << '\n';
        }
    }

    TIR_Context tir_context { .global = global };
    tir_context.compile_all();

    // Print the TIR
    if (debug_output) {
        wcout << red << "\n--------- TIR ---------\n" << resetstyle;
        for (auto& kvp : tir_context.fns)
            kvp.value->print();
        wcout.flush();
    }

    if (debug_output) {
        wcout << red << "\n------- LLVM IR -------\n" << resetstyle;
        // The LLVM IR is pritned by the compile_all function
    }
    fflush(stdout);

    /*
    T2L_Context t2l_context(tir_context);
    t2l_context.compile_all();

    const char* object_filename = t2l_context.output_object();

    if (output_type == OUTPUT_LINKED_EXECUTABLE) {
        // TODO ENCODING
        std::string of = object_filename;
        std::wstring object_filename_w(of.begin(), of.end());

        std::string ouf = output_file;
        std::wstring output_filename_w(ouf.begin(), ouf.end());

        if (has_msvc_linker) {
            link(msvc_linker, object_filename_w, output_filename_w);
        }
        else if (has_gnu_ld) {
            link(gnu_ld, object_filename_w, output_filename_w);
        }
        else if (has_lld) {
            link(lld, object_filename_w, output_filename_w);
        }
    }
    */


    // Execute the main function
    TIR_ExecutionJob *tir_exec_job = new TIR_ExecutionJob(&tir_context);
    global.jobs.push(tir_exec_job);
    tir_exec_job->on_done = main_exec_job_on_done;

    for (auto& kvp : tir_context.fns) {
        if (kvp.key->name && !strcmp(kvp.key->name, "main")) {
            tir_exec_job->call(kvp.value);
            break;
        }
    }

    global.run_jobs();
    return 0;
}

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

struct MainExecJob : TIR_ExecutionJob {
    void on_complete(void *value) override {
        wcout << "main returned " << (i64)value << "\n";
    }

    std::wstring get_name() override { return L"MainExecJob"; };

    MainExecJob(TIR_Context *tir_context) : TIR_ExecutionJob(tir_context) {}
};

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

    // Print the source code
    if (print_ast) {
        wcout << red << "--------- AST ---------\n" << resetstyle;
        for (const auto& decl : global.declarations) {
            print(wcout, decl.value, true);
            wcout << '\n';
        }
    }

    JobGroup *all_tir_compiled_job = new JobGroup(global, L"all_tir_compiled");
    global.add_job(all_tir_compiled_job);
    
    TIR_Context tir_context { .global = global };

    for (auto &decl : global.declarations) {
        TypeCheckJob *j = new TypeCheckJob(global, decl.value);
        global.add_job(j);
        //typecheck_all_job->add_dependency(j);

        if (decl.value->nodetype == AST_FN) {
            Job *j2 = tir_context.compile_fn((AST_Fn*)decl.value, j);
            all_tir_compiled_job->add_dependency(j2);
        } else if (decl.value->nodetype == AST_VAR) {
            tir_context.append_global((AST_Var*)decl.value);
        }
    }

    // Execute the main function
    if (exec_main) {
        MainExecJob *main_exec_job = new MainExecJob(&tir_context);
        main_exec_job->add_dependency(all_tir_compiled_job);
        global.add_job(main_exec_job);

        for (auto& kvp : tir_context.fns) {
            if (kvp.key->name && !strcmp(kvp.key->name, "main")) {
                arr<void*> _args;
                main_exec_job->call(kvp.value, _args);
                break;
            }
        }
    }

    tir_context.compile_all();
    global.run_jobs();

    // Print the TIR
    if (print_tir) {
        wcout << red << "\n--------- TIR ---------\n" << resetstyle;
        for (auto& kvp : tir_context.fns)
            kvp.value->print();
        wcout.flush();
    }

    // if (debug_output) {
    //     wcout << red << "\n------- LLVM IR -------\n" << resetstyle;
    //     // The LLVM IR is pritned by the compile_all function
    // }
    // fflush(stdout);

    // /*
    //    T2L_Context t2l_context(tir_context);
    //    t2l_context.compile_all();

    //    const char* object_filename = t2l_context.output_object();

    //    if (output_type == OUTPUT_LINKED_EXECUTABLE) {
    // // TODO ENCODING
    // std::string of = object_filename;
    // std::wstring object_filename_w(of.begin(), of.end());

    // std::string ouf = output_file;
    // std::wstring output_filename_w(ouf.begin(), ouf.end());

    // if (has_msvc_linker) {
    // link(msvc_linker, object_filename_w, output_filename_w);
    // }
    // else if (has_gnu_ld) {
    // link(gnu_ld, object_filename_w, output_filename_w);
    // }
    // else if (has_lld) {
    // link(lld, object_filename_w, output_filename_w);
    // }
    // }
    // */


    return 0;
}

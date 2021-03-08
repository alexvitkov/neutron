#include "backend/llvm/llvm.h" // include this first
#include "context.h"
#include "tir.h"
#include "typer.h"
#include "ast.h"
#include "cmdargs.h"
#include "error.h"
#include "linker.h"
#include "util.h"

AST_GlobalContext global;


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
        for (auto& err : global.errors)
            print_err(global, err);
        exit(1);
    }

    if (print_ast) {
        wcout << red << "--------- AST ---------\n" << resetstyle;
        for (const auto& decl : global.declarations) {
            print(wcout, decl.value, true);
            wcout << '\n';
        }
    }

    JobGroup _all_tir_compiled_job (global, L"all_tir_compiled");
    HeapJob *all_tir_compiled_job = _all_tir_compiled_job.heapify<JobGroup>();
    global.add_job(all_tir_compiled_job);
    
    TIR_Context tir_context { .global = global };
    global.tir_context = &tir_context;

    for (auto &decl : global.fns_to_declare) {
        TypeCheckJob _j (decl.scope, decl.fn);
        HeapJob *j = _j.heapify<TypeCheckJob>();
        global.add_job(j);

        HeapJob *j2 = tir_context.compile_fn(decl.fn, j);
        all_tir_compiled_job->add_dependency(j2);
    }

    for (auto &decl : global.declarations) {
        TypeCheckJob _j (global, decl.value);
        HeapJob *j = _j.heapify<TypeCheckJob>();
        global.add_job(j);

        if (decl.value->nodetype == AST_VAR) {
            tir_context.append_global((AST_Var*)decl.value);
        }
    }

    // Execute the main function
    if (exec_main) {
        MainExecJob _main_exec_job (&tir_context);
    HeapJob *main_exec_job = _main_exec_job.heapify<MainExecJob>();
        main_exec_job->add_dependency(all_tir_compiled_job);
        global.add_job(main_exec_job);

        for (auto& kvp : tir_context.fns) {
            if (kvp.key->name && !strcmp(kvp.key->name, "main")) {
                arr<void*> _args;
                ((MainExecJob*)main_exec_job->job())->call(kvp.value, _args);
                break;
            }
        }
    }


    if (!global.run_jobs()) {
        wcout << red << global.jobs_count << " Jobs didn't complete:\n" << resetstyle;

        for (auto &kvp : global.jobs_by_id) {
            if (!(kvp.value->job()->flags & JOB_DONE)) {
                if (!(kvp.value->job()->flags & JOB_ERROR))
                    wcout << "    - HANG " << kvp.value->job()-> id << ": " << kvp.value->job()->get_name() << "\n";
            }
        }

        for (auto &kvp : global.jobs_by_id) {
            if (!(kvp.value->job()->flags & JOB_DONE)) {
                if ((kvp.value->job()->flags & JOB_ERROR) == JOB_ERROR)
                    wcout << red << "    - FAIL " << kvp.value->job()-> id << ": " << resetstyle << kvp.value->job()->get_name() << "\n";
            }
        }


        return 1;
    }

    tir_context.compile_all();

    if (print_tir) {
        wcout << red << "\n--------- TIR ---------\n" << resetstyle;
        for (auto& kvp : tir_context.fns)
            kvp.value->print();
        wcout.flush();
    }


    return 0;
    T2L_Context t2l_context(tir_context);
    t2l_context.compile_all(all_tir_compiled_job);

    const char* object_filename = t2l_context.output_object();

    if (output_type == OUTPUT_LINKED_EXECUTABLE) {
        // TODO ENCODING
        std::string of = object_filename;
        std::wstring object_filename_w(of.begin(), of.end());

        std::string ouf = output_file;
        std::wstring output_filename_w(ouf.begin(), ouf.end());

        if (has_msvc_linker) {
            link(msvc_linker, object_filename_w, output_filename_w);
        } else if (has_gnu_ld) {
            link(gnu_ld, object_filename_w, output_filename_w);
        } else if (has_lld) {
            link(lld, object_filename_w, output_filename_w);
        }
    }



    return 0;
}

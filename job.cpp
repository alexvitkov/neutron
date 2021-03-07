#include "context.h"
#include "ast.h"
#include "typer.h"
#include "tir.h"
#include "tir_builtins.h"
#include "cast.h"
#include <sstream>
#include <iostream>

void HeapJob::add_dependency(HeapJob* dependency) {
    assert(!(dependency->job()->flags & JOB_DONE));

    if (dependency->job()->flags & JOB_ERROR) {
        UNREACHABLE;
        job()->set_error_flag();
        return;
    }

    dependencies_left++;
    dependency->dependent_jobs.push(job()->id);
    if (debug_jobs) {
        wcout << job()->get_name() << dim << " depends on " << resetstyle << dependency->job()->get_name() << "\n";
        wcout.flush();
    }

}

void finish_job(AST_GlobalContext &global, HeapJob *finished_job) {
    if (finished_job->job()->on_complete) {
        finished_job->job()->on_complete(finished_job->job(), nullptr);
    }
    if (debug_jobs) {
        wcout << dim << "Finished " << resetstyle << finished_job->job()->get_name() << "\n";
        wcout.flush();
    }
    global.jobs_count--;
    finished_job->job()->flags = (JobFlags)(finished_job->job()->flags | JOB_DONE);

    for (u64 dependent_job_id : finished_job->dependent_jobs) {

        HeapJob *dependent_job = global.jobs_by_id[dependent_job_id];

        dependent_job->dependencies_left --;
        if (dependent_job->dependencies_left == 0)
            global.ready_jobs.push(dependent_job);
    }
}

Job::Job(AST_GlobalContext &global) : global(global) {
    id = global.next_job_id++;
}

void AST_GlobalContext::send_message(arr<HeapJob*> &receivers, Message *msg) {
    if (debug_jobs) {
        wcout << dim << "Sending message " << resetstyle << msg->msgtype << "\n";
        wcout.flush();
    }

    for (u32 i = 0; i < receivers.size; ) {
        if (receivers[i]->job()->flags & JOB_DONE) {
            receivers.delete_unordered(i);
        } else if (receivers[i]->job()->run(msg)) {
            finish_job(*this, receivers[i]);
            receivers.delete_unordered(i);
        } else {
            // delete_unordered places the last job at the index of the old
            // element, so we don't increment 'i' if we've called it
            i++;
        }
    }
}

void AST_GlobalContext::add_job(HeapJob *job) {
    jobs_count ++;
    jobs_by_id[job->job()->id] = job;

    for (HeapJob *j : ready_jobs) {
        if (j == job)
            assert(!"Adding the same job twice");
    }

    if (debug_jobs) {
        wcout << dim << "Adding " << resetstyle << job->job()->id << ":" << job->job()->get_name() << "\n";
        wcout.flush();
    }
    ready_jobs.push(job);
}

bool AST_GlobalContext::run_jobs() {
    while (ready_jobs.size) {
        HeapJob *job = ready_jobs.pop();

        if (job->dependencies_left != 0 || (job->job()->flags & (JOB_DONE | JOB_WAITING_MSG)))
            continue;

        if (job->job()->flags & JOB_ERROR) {
            for (u64 dependent_job_id : job->dependent_jobs) {
                HeapJob *j = global.jobs_by_id[dependent_job_id]; 
                j->job()->set_error_flag();
            }
            continue;
        }

        if (job->job()->run(nullptr)) {
            finish_job(*this, job);
        }
    }

    return jobs_count == 0;
}

ResolveJob::ResolveJob(AST_Context &ctx, AST_UnresolvedId **id) 
    : Job(ctx.global), unresolved_id(id), context(&ctx) 
{
    flags = (JobFlags)(flags | JOB_WAITING_MSG);
}


struct MatchCallJob : Job {
    AST_Context *context;
    AST_Fn      *fn;
    AST_Call    *fncall;
    int          prio;

    arr<AST_Value*> casted_args;

    u64 parent_job;

    inline MatchCallJob(AST_Context *ctx, AST_Fn *fn, AST_Call *call, JobOnCompleteCallback on_complete) 
        : Job(ctx->global), 
          context(ctx), 
          fn(fn), 
          fncall(call), 
          casted_args(fncall->args.size)
    {
        casted_args.size = fncall->args.size;
        this->on_complete = on_complete;
        prio = 0;
    }

    bool run(Message *msg) override {
        AST_FnType *fntype = fn->fntype();

        MUST (fntype->param_types.size >= fncall->args.size);
        if (!fntype->is_variadic)
            MUST (fntype->param_types.size == fncall->args.size);

        if (prio == 0) {
            prio = 1000;
            bool wait = false;
            for (u32 i = 0; i < fntype->param_types.size; i++) {
                AST_Type *paramtype = fntype->param_types[i];
                AST_Type *argtype = fncall->args[i]->type;

                CastJob cast_job(context, fncall->args[i], paramtype, [](Job *self, Job *parent) {
                    MatchCallJob *p = (MatchCallJob*)parent;
                    p->casted_args[(u64)self->input] = (AST_Value*)self->result;
                });

                cast_job.input = (void*)(u64)i;
                if (!run_child<MatchCallJob, CastJob>(cast_job))
                    wait = true;
            }
            if (wait)
                return false;
        } 
        
        return true;
    }

    std::wstring get_name() override {
        return L"MatchCallJob";
    }
};


bool ResolveJob::spawn_match_job(AST_Fn *fn) {
    pending_matches++;

    MatchCallJob the_job(context, fn, this->fncall, [](Job *_self, Job *_parent) {
        MatchCallJob *self = (MatchCallJob*)_self;
        ResolveJob *parent = (ResolveJob*)_parent;

        parent->pending_matches --;

        if (parent->prio < self->prio) {
            if (!parent->new_args.size) {
                parent->new_args.realloc(parent->fncall->args.size);
                parent->new_args.size = (parent->fncall->args.size);
            }
            for (int i = 0; i < parent->fncall->args.size; i++)
                parent->new_args[i] = self->casted_args[i];

            parent->new_fn = self->fn;
            parent->prio   = self->prio;

        } else if (parent->prio == self->prio) {
            // TODO ERROR
            NOT_IMPLEMENTED();
        }

        if (parent->pending_matches == 0) {
            if (parent->context && parent->context->closed) {
                parent->context = parent->context->parent;
                parent->read_scope();
            }
        }
    });

    the_job.input = 0;
    run_child<ResolveJob, MatchCallJob>(the_job);
    the_job.input = (void*)1;
    return true;
}


DeclarationKey ResolveJob::get_decl_key() {
    DeclarationKey key = {};

    if (fncall) {
        if (fncall->fn) {
            key.name = ((AST_UnresolvedId*)fncall->fn)->name;
        } else {
            assert (fncall->op);
            key.op = fncall->op;
        }
    } else {
        assert(unresolved_id);
        assert((*unresolved_id) IS AST_UNRESOLVED_ID);
        key.name = (*unresolved_id)->name;
    }
    return key;
}

bool key_compatible(DeclarationKey &lhs, DeclarationKey &rhs) {
    MUST (lhs.name);
    MUST (rhs.name);
    MUST (!strcmp(lhs.name, rhs.name));
    return true;
}

bool ResolveJob::read_scope() {
    if (!context) {
        if (unresolved_id) {
            // TODO ERROR
            set_error_flag();
            return false;
        } else if (prio > 0) {
            for (int i = 0; i < fncall->args.size; i++)
                fncall->args[i] = new_args[i];
            fncall->fn = new_fn;
            fncall->type = ((AST_FnType*)fncall->fn->type)->returntype;
            job_done();
            return true;
        } else {
            // TODO ERROR
            set_error_flag();
            return false;
        }
    }
         
    context->subscribers.push(heapify<ResolveJob>());

    if (unresolved_id) {
        AST_Node *decl;
        MUST (context->declarations.find({ .name = (*unresolved_id)->name }, &decl));
        *(AST_Node**)unresolved_id = decl;
        job_done();
        return true;
    } else if (fncall->op) {
        switch (fncall->args.size) {
            case 2: {
                TIR_Builder *builder = get_builder(
                    fncall->op, 
                    fncall->args[0]->type, 
                    fncall->args[1]->type,
                    &fncall->type
                );
                if (!builder) {
                    set_error_flag();
                    return false;
                }
                fncall->builder = builder;
                job_done();
                return true;
            }
            case 1: { NOT_IMPLEMENTED(); }
            default: UNREACHABLE;
        }
    } else {
        DeclarationKey key = get_decl_key();

        for (auto &kvp : context->declarations)
            if (kvp.value IS AST_FN && key_compatible(key, kvp.key))
                spawn_match_job((AST_Fn*)kvp.value);

        return false;
    }
}


bool ResolveJob::run(Message *msg) {
    if (!msg) {
        do {
            if (read_scope())
                return true;
            if (flags & JOB_ERROR)
                return false;
        } while (context && context->closed && (context = context->parent));

        return flags & JOB_DONE;
    }

    switch (msg->msgtype) {
        case MSG_NEW_DECLARATION: {
            NewDeclarationMessage *decl = (NewDeclarationMessage*)msg;
            MUST (decl->context == context);

            DeclarationKey key = get_decl_key();

            if (key_compatible(key, decl->key)) {
                if (unresolved_id) {
                    *(AST_Node**)unresolved_id = decl->node;
                    return true;
                } else {
                    if (decl->node IS AST_FN)
                        spawn_match_job((AST_Fn*)decl->node);
                    return false;
                }
            }
        }

        case MSG_SCOPE_CLOSED: {
            ScopeClosedMessage *sc = (ScopeClosedMessage*)msg;

            MUST (pending_matches == 0);
            MUST (sc->scope == context);

            context = context->parent;
            return read_scope();
        }

        default:
            UNREACHABLE;
    }
}

void Job::error(Error err) {
    wcout << "Error from job " << get_name() << ":\n";
    global.error(err);
    print_err(global, err);

    set_error_flag();

    HeapJob *this_on_heap;
    if (global.jobs_by_id.find(id, &this_on_heap)) {
        for (u64 dependent_job_id : this_on_heap->dependent_jobs) {
            HeapJob *job = global.jobs_by_id[dependent_job_id];
            job->job()->set_error_flag();
        }
    }
}

JobGroup::JobGroup(AST_GlobalContext &ctx, std::wstring name) 
    : Job(ctx), name(name) { }

bool JobGroup::run(Message *msg) {
    return !msg;
}
std::wstring JobGroup::get_name() {
    return L"JobGroup<" + name + L">";
}

std::wstring ResolveJob::get_name() {
    std::wostringstream stream;

    if (unresolved_id) {
        stream << "ResolveJob<" << (*unresolved_id)->name << L">";
    } else {
        stream << "ResolveJob<" << fncall << L">";
    }
    return stream.str();
}


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

void AST_GlobalContext::send_message(Message *msg) {
    arr<HeapJob*>& receivers = subscribers[msg->msgtype];
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

bool ResolveJob::spawn_match_job(AST_Fn *fn) {
    AST_FnType *fntype = fn->fntype();

    MUST (fntype->param_types.size >= fncall->args.size);

    if (!fntype->is_variadic)
        MUST (fntype->param_types.size == fncall->args.size);

    int prio = 1000;
    arr<AST_Value*> casted_args(fncall->args.size);
    casted_args.size = fncall->args.size;

    JobGroup *wait_group = nullptr;

    for (u32 i = 0; i < fntype->param_types.size; i++) {
        AST_Type *paramtype = fntype->param_types[i];
        AST_Type *argtype = fncall->args[i]->type;

        CastJob cast_job(context, fncall->args[i], paramtype);
        HeapJob *cast_heap_job = cast_job.run_stackjob<CastJob>();
        if (cast_heap_job) {
            NOT_IMPLEMENTED();
            if (wait_group) {}
        } else if (cast_job.flags & JOB_DONE) {
            casted_args[i] = cast_job.result;
            continue;
        }
        return false;
    }

    pending_matches ++;

    MatchCallJobOverMessage matched_msg;
    matched_msg.msgtype = MSG_FN_MATCHED;
    matched_msg.priority = prio;
    matched_msg.fncall = fncall;
    matched_msg.fn = fn;

    for (u32 i = 0; i < fncall->args.size; i++)
        matched_msg.casted_args.push(casted_args[i]);

    global.send_message(&matched_msg);
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

bool ResolveJob::run(Message *msg) {
    if (!msg) {
        DeclarationKey key = get_decl_key();
        do { 
            if (unresolved_id) {
                AST_Node *decl;
                MUST (context->declarations.find({ .name = (*unresolved_id)->name }, &decl));
                *(AST_Node**)unresolved_id = decl;
                return true;
            } else {
                
                if (fncall->op) {
                    switch (fncall->args.size) {
                        case 2: {
                            TIR_Builder *builder = get_builder(
                                fncall->op, 
                                fncall->args[0]->type, 
                                fncall->args[1]->type,
                                &fncall->type
                            );

                            MUST_OR_FAIL_JOB (builder);

                            fncall->builder = builder;
                            return true;
                        }
                        case 1: {
                            NOT_IMPLEMENTED();
                            /*
                            MUST_OR_FAIL_JOB (builtin_unary_ops.find({ 
                                fncall->op, 
                                fncall->args[0]->type, 
                            }, &fn));

                            fncall->tir_fn = fn;
                            fncall->type = fncall->args[0]->type;
                            return true;
                            */
                        }
                        default:
                            UNREACHABLE;
                    }
                }

                for (auto &kvp : context->declarations)
                    if (kvp.value IS AST_FN && key_compatible(key, kvp.key))
                        spawn_match_job((AST_Fn*)kvp.value);

            }
        } while (context->closed && (context = context->parent));

        if (!context) {
            assert (pending_matches == 0);
            set_error_flag();
        }
        return false;
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
            if (pending_matches > 0)
                return false;

            ScopeClosedMessage *sc = (ScopeClosedMessage*)msg;
            if (sc->scope == context) {
                // TODO 717
                context = context->parent;
                if (!context) {
                    if (flags & JOB_WAITING_MSG) {
                        if (this->fncall && this->prio > 0) {
                            for (int i = 0; i < fncall->args.size; i++)
                                fncall->args[i] = new_args[i];
                            fncall->fn = new_fn;
                            fncall->type = ((AST_FnType*)fncall->fn->type)->returntype;
                            return true;
                        }
                        error({ .code = ERR_NOT_DEFINED, .nodes = { unresolved_id ? (AST_Value*)*unresolved_id : fncall } });
                        return false;
                    } else {
                        return false;
                    }
                } else {
                    return run(nullptr);
                }
            }
            return false;
        }

        case MSG_FN_MATCHED: {
            MatchCallJobOverMessage *match_msg = (MatchCallJobOverMessage*)msg;
            if (match_msg->fncall != fncall)
                return false;

            pending_matches --;

            if (prio < match_msg->priority) {

                if (prio == 0)
                    new_args.realloc(fncall->args.size);

                prio = match_msg->priority;

                for (int i = 0; i < fncall->args.size; i++)
                    new_args[i] = match_msg->casted_args[i];

                new_fn = match_msg->fn;

            } else if (prio == match_msg->priority) {
                // TODO ERROR
                NOT_IMPLEMENTED();
            }

            if (pending_matches == 0) {
                if (context && context->closed) {
                    context = context->parent;
                    if (context) {
                        assert(!run(nullptr));
                        return false;
                    }
                }
                if (!context) {
                    if (prio > 0) {
                        for (int i = 0; i < fncall->args.size; i++)
                            fncall->args[i] = new_args[i];
                        fncall->fn = new_fn;
                        fncall->type = ((AST_FnType*)fncall->fn->type)->returntype;
                        return true;
                    } else {
                        // TODO ERROR
                        NOT_IMPLEMENTED();
                    }
                } else {
                    return false;
                }
            }

            return false;
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

void HeapJob::subscribe(MessageType msgtype) {
    if (debug_jobs) {
        wcout << job()->get_name() << dim << " subscribed to " << resetstyle << msgtype << "\n";
        wcout.flush();
    }
    job()->global.subscribers[msgtype].push(this);
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


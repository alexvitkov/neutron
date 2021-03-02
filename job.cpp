#include "context.h"
#include "ast.h"
#include "typer.h"
#include "cast.h"
#include <sstream>
#include <iostream>

void Job::add_dependency(Job* dependency) {
    assert(!(dependency->flags & JOB_DONE));

    if (dependency->flags & JOB_ERROR) {
        set_error_flag();
        return;
    }

    dependencies_left++;
    dependency->dependent_jobs.push(id);
    if (debug_jobs) {
        wcout << get_name() << dim << " depends on " << resetstyle << dependency->get_name() << "\n";
        wcout.flush();
    }

}

void finish_job(AST_GlobalContext &global, Job *finished_job) {
    if (debug_jobs) {
        wcout << dim << "Finished " << resetstyle << finished_job->get_name() << "\n";
        wcout.flush();
    }
    global.jobs_count--;
    finished_job->flags = (JobFlags)(finished_job->flags | JOB_DONE);

    for (u64 dependent_job_id : finished_job->dependent_jobs) {

        Job *dependent_job = global.jobs_by_id[dependent_job_id];

        dependent_job->dependencies_left --;
        if (dependent_job->dependencies_left == 0)
            global.ready_jobs.push(dependent_job);
    }
}

Job::Job(AST_GlobalContext &global) : global(global) {
    id = global.next_job_id++;
}

void AST_GlobalContext::send_message(Message *msg) {
    arr<Job*>& receivers = subscribers[msg->msgtype];
    if (debug_jobs) {
        wcout << dim << "Sending message " << resetstyle << msg->msgtype << "\n";
        wcout.flush();
    }

    for (u32 i = 0; i < receivers.size; ) {
        if (receivers[i]->flags & JOB_DONE) {
            receivers.delete_unordered(i);
        } else if (receivers[i]->run(msg)) {
            finish_job(*this, receivers[i]);
            receivers.delete_unordered(i);
        } else {
            // delete_unordered places the last job at the index of the old
            // element, so we don't increment 'i' if we've called it
            i++;
        }
    }
}

void AST_GlobalContext::add_job(Job *job) {
    jobs_count ++;
    jobs_by_id[job->id] = job;

    for (Job *j : ready_jobs) {
        if (j == job)
            assert(!"Adding the same job twice");
    }

    if (debug_jobs) {
        wcout << dim << "Adding " << resetstyle << job->id << ":" << job->get_name() << "\n";
        wcout.flush();
    }
    ready_jobs.push(job);
}

bool AST_GlobalContext::run_jobs() {
    while (ready_jobs.size) {
        Job *job = ready_jobs.pop();

        if (job->dependencies_left != 0 || (job->flags & JOB_DONE))
            continue;

        if (job->flags & JOB_ERROR) {
            for (u64 dependent_job_id : job->dependent_jobs) {
                Job *j = global.jobs_by_id[dependent_job_id]; 
                j->set_error_flag();
            }
            continue;
        }

        if (job->run(nullptr)) {
            finish_job(*this, job);
        }
    }

    return jobs_count == 0;
}

ResolveJob::ResolveJob(AST_Context &ctx, AST_UnresolvedId **id) 
    : Job(ctx.global), unresolved_id(id), context(&ctx) 
{
    // TODO JOB 717 - we add a fake dependency to make sure the job doesnt get called
    // we're actually waiting for a message
    this->dependencies_left ++;
}

void ResolveJob::spawn_match_job(AST_Fn *fn) {
    MatchCallJob *match_job = new MatchCallJob(context->global, fncall, fn);
    global.add_job(match_job);
    add_dependency(match_job);
    pending_matches ++;
}

DeclarationKey ResolveJob::get_decl_key() {
    DeclarationKey key = {};

    if (fncall->fn) {
        assert(fncall->fn IS AST_UNRESOLVED_ID);
        key.name = ((AST_UnresolvedId*)fncall->fn)->name;
    } else {
        key.op = fncall->op;
    }
    return key;
}

bool key_compatible(DeclarationKey &lhs, DeclarationKey &rhs) {
    MUST (lhs.name);
    MUST (rhs.name);
    MUST (strcmp(lhs.name, rhs.name));
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
                    if (dependencies_left == 1) {
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
            if (match_msg->job->fncall != fncall)
                return false;

            pending_matches --;

            if (!(match_msg->job->flags & JOB_FALSE)) {
                wcout << "prio:" << match_msg->job->priority << "\n";

                if (prio < match_msg->job->priority) {

                    if (prio == 0)
                        new_args.realloc(fncall->args.size);

                    prio = match_msg->job->priority;

                   for (int i = 0; i < fncall->args.size; i++)
                       new_args[i] = match_msg->job->casted_args[i];

                   new_fn = match_msg->job->fn;

                } else if (prio == match_msg->job->priority) {
                    // TODO ERROR
                    NOT_IMPLEMENTED();
                }
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
    for (u64 dependent_job_id : dependent_jobs) {
        Job *job = global.jobs_by_id[dependent_job_id];
        job->set_error_flag();
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

void Job::subscribe(MessageType msgtype) {
    if (debug_jobs) {
        wcout << get_name() << dim << " subscribed to " << resetstyle << msgtype << "\n";
        wcout.flush();
    }
    global.subscribers[msgtype].push(this);
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


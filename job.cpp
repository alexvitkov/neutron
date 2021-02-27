#include "context.h"
#include "ast.h"
#include "typer.h"
#include <sstream>
#include <iostream>

void Job::add_dependency(Job* dependency) {
    assert(!(dependency->flags & JOB_DONE));

    if (dependency->flags & JOB_ERROR) {
        flags = (JobFlags)(flags | JOB_ERROR);
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
        wcout << dim << "Adding " << resetstyle << job->get_name() << "\n";
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
                j->flags = (JobFlags)(j->flags | JOB_ERROR);
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
    : Job(ctx.global), id(id), context(&ctx) 
{
    // TODO JOB - we add a fake dependency to make sure the job doesnt get called
    // we're actually waiting for a message
    this->dependencies_left ++;
}

bool ResolveJob::run(Message *msg) {
    if (!msg) {
        if (id) {
            AST_Node *decl;
            MUST (context->declarations.find({ .name = (*id)->name }, &decl));
            *(AST_Node**)id = decl;
            return true;
        } else {
            AST_UnresolvedId *id = (AST_UnresolvedId*)fncall->fn;
            assert(id IS AST_UNRESOLVED_ID);

            for (auto &kvp : context->declarations) {
                wcout << "aaa\n";
                if (!strcmp(id->name, kvp.key.name) && match_fn_call(*context, fncall, (AST_Fn*)kvp.value))
                    return true;
            }

            while (context->closed) {
                context = context->parent;

                if (!context) {
                    error({ .code = ERR_NOT_DEFINED, .nodes = { fncall->fn } });
                    return false;
                }

                for (auto &kvp : context->declarations) {
                    if (!strcmp(id->name, kvp.key.name) && match_fn_call(*context, fncall, (AST_Fn*)kvp.value))
                        return true;
                }
            }

            return false;
        }
    }

    switch (msg->msgtype) {
        case MSG_NEW_DECLARATION: {
            NewDeclarationMessage *decl = (NewDeclarationMessage*)msg;
            MUST (decl->context == context);

            if (id) {
                MUST (!strcmp(decl->key.name, (*id)->name));
                *(AST_Node**)id = decl->node;
                return true;
            } else {
                MUST (decl->node->nodetype == AST_FN)
                return match_fn_call(*context, fncall, (AST_Fn*)decl->node);
            }
        }
        case MSG_SCOPE_CLOSED: {
            ScopeClosedMessage *sc = (ScopeClosedMessage*)msg;
            if (sc->scope == context) {
                context = context->parent;
                if (!context) {
                    error({ .code = ERR_NOT_DEFINED, .nodes = { id ? (AST_Value*)*id : fncall } });
                    return false;
                } else {
                    return run(nullptr);
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

    flags = (JobFlags)(flags | JOB_ERROR);
    for (u64 dependent_job_id : dependent_jobs) {
        Job *job = global.jobs_by_id[dependent_job_id];
        job->flags = (JobFlags)(job->flags | JOB_ERROR);
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

    if (id) {
        stream << "ResolveJob<" << (*id)->name << L">";
    } else {
        stream << "ResolveJob<" << fncall << L">";
    }
    return stream.str();
}


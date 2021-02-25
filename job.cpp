#include "context.h"
#include "ast.h"
#include <sstream>
#include <iostream>

void Job::add_dependency(Job* dependency) {
    assert(!(dependency->flags & JOB_DONE));

    if (dependency->flags & JOB_ERROR) {
        flags = (JobFlags)(flags | JOB_ERROR);
        return;
    }

    dependencies_left++;
    dependency->dependent_jobs.push(this);
#ifdef DEBUG_JOBS
    wcout << get_name() << dim << " depends on " << resetstyle << dependency->get_name() << "\n";
#endif

}

std::wstring Job::get_name() {
    return L"generic_job";
}

Job::Job(AST_GlobalContext *global) : global(global) {
}

void AST_GlobalContext::send_message(Message *msg) {
    arr<Job*>& receivers = subscribers[msg->msgtype];
    // wcout << "Sending message " << msg->msgtype << "\n";

    for (u32 i = 0; i < receivers.size; ) {
        if (receivers[i]->flags & JOB_DONE || receivers[i]->_run(msg)) {
            receivers.delete_unordered(i);
        } else {
            i++;
        }
    }
}

void AST_GlobalContext::add_job(Job *job) {
    for (Job *j : ready_jobs) {
        if (j == job)
            assert(!"Adding the same job twice");
    }

#ifdef DEBUG_JOBS
    wcout << dim << "Adding " << resetstyle << job->get_name() << "\n";
#endif
    ready_jobs.push(job);
}

bool AST_GlobalContext::run_jobs() {
    while (ready_jobs.size) {
        Job *job = ready_jobs.pop();

        if (job->dependencies_left != 0 || (job->flags & JOB_DONE))
            continue;

        if (job->flags & JOB_ERROR) {
            for (Job * j : job->dependent_jobs) {
                j->flags = (JobFlags)(j->flags | JOB_ERROR);
            }
            continue;
        }

        if (job->_run(nullptr)) {
#ifdef DEBUG_JOBS
            wcout << dim << "Finished " << resetstyle << job->get_name() << "\n";
#endif
            jobs_count--;
            job->flags = (JobFlags)(job->flags | JOB_DONE);

            for (Job *dependent_job : job->dependent_jobs) {
                dependent_job->dependencies_left --;
                if (job->dependencies_left == 0)
                    ready_jobs.push(dependent_job);
            }
        }
    }

    return jobs_count == 0;
}

ResolveJob::ResolveJob(AST_UnresolvedId **id, AST_Context *ctx) 
    : Job(ctx->global), id(id), context(ctx) 
{
    // TODO JOB - we add a fake dependency to make sure the job doesnt get called
    // we're actually waiting for a message
    this->dependencies_left ++;
}

bool ResolveJob::_run(Message *msg) {
    if (!msg) {
        AST_Node *decl;
        MUST (context->declarations.find({ .name = (*id)->name }, &decl));

        *(AST_Node**)id = decl;
        return true;
    }

    switch (msg->msgtype) {
        case MSG_NEW_DECLARATION: {
            NewDeclarationMessage *decl = (NewDeclarationMessage*)msg;
            if (decl->context != context)
                return false;

            if (!strcmp(decl->name, (*id)->name)) {
                *(AST_Node**)id = decl->node;
                return true;
            }

            return false;
        }
        case MSG_SCOPE_CLOSED: {
            ScopeClosedMessage *sc = (ScopeClosedMessage*)msg;
            if (sc->scope == context) {
                context = context->parent;
                if (!context) {
                    error({
                        .code = ERR_NOT_DEFINED,
                        .node_ptrs = { (AST_Node**)id }
                    });
                    return false;
                } else {
                    return _run(nullptr);
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
    global->error(err);
    print_err(*global, err);

    flags = (JobFlags)(flags | JOB_ERROR);
    for (Job *job : dependent_jobs) {
        job->flags = (JobFlags)(job->flags | JOB_ERROR);
    }
}

JobGroup::JobGroup(AST_GlobalContext *ctx, std::wstring name) 
    : Job(ctx), name(name) { }

bool JobGroup::_run(Message *msg) {
    return !msg;
}
std::wstring JobGroup::get_name() {
    return L"JobGroup<" + name + L">";
}

void Job::subscribe(MessageType msgtype) {
    global->subscribers[msgtype].push(this);
}

std::wstring ResolveJob::get_name() {
    std::wostringstream stream;
    stream << "ResolveJob<" << (*id)->name << L">";
    return stream.str();
}


#include "context.h"

#include <iostream>

void Job::add_dependency(Job* dependency) {
    dependencies_left++;
    dependency->dependent_jobs.push(this);
    // wcout << get_name() << dim << " depends on " << resetstyle << dependency->get_name() << "\n";
}

std::wstring Job::get_name() {
    return L"generic_job";
}

bool Job::receive_message(Message *msg) {
    return false;
}

Job::Job(AST_GlobalContext *global) : global(global) {
    global->jobs_count++;
    global->ready_jobs.push(this);
}

void AST_GlobalContext::send_message(Message *msg) {
    arr<Job*> receivers = subscribers[msg->msgtype];

    for (u32 i = 0; i < receivers.size; ) {
        if (receivers[i]->flags & JOB_DONE || receivers[i]->receive_message(msg)) {
            receivers.delete_unordered(i);
        } else {
            i++;
        }
    }
}

bool AST_GlobalContext::run_jobs() {
    while (ready_jobs.size) {
        Job *job = ready_jobs.pop();

        if (job->dependencies_left != 0)
            continue;

        if (job->run()) {
            // wcout << dim << "finished " << resetstyle << job->get_name() << "\n";
            jobs_count--;
            for (Job *dependent_job : job->dependent_jobs) {
                dependent_job->dependencies_left --;
                if (job->dependencies_left == 0)
                    ready_jobs.push(dependent_job);
            }
        }
    }

    return jobs_count == 0;
}


ResolveJob::ResolveJob(AST_UnresolvedId *id, AST_Context *ctx) 
    : Job(ctx->global), id(id), context(ctx) 
{
    // TODO JOB - we add a fake dependency to make sure the job doesnt get called
    // we're actually waiting for a message
    this->dependencies_left ++;
}

bool ResolveJob::run() {
    UNREACHABLE;
}

bool ResolveJob::receive_message(Message *msg) {
    assert (msg->msgtype == MSG_NEW_DECLARATION);

    NewDeclarationMessage *decl = (NewDeclarationMessage*)msg;
    if (decl->context != context)
        return false;



    return false;
}

void Job::subscribe(MessageType msgtype) {
    global->subscribers[msgtype].push(this);
}

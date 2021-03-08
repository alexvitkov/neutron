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
        } else {
            auto asdf = receivers[i]->job()->run(msg);
            if (asdf) {
                finish_job(*this, receivers[i]);
                receivers.delete_unordered(i);
            } else {
                // delete_unordered places the last job at the index of the old
                // element, so we don't increment 'i' if we've called it
                i++;
            }
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

JobGroup::JobGroup(AST_GlobalContext &ctx, JobOnCompleteCallback on_complete) : Job(ctx) {
    this->on_complete = on_complete;
}

bool JobGroup::run(Message *msg) {
    return !msg;
}

std::wstring JobGroup::get_name() {
    return L"JobGroup<" + name + L">";
}


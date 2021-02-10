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

void AST_GlobalContext::add_job(Job *job) {
    jobs_count++;
    ready_jobs.push(job);
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

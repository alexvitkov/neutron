#include "context.h"

#include <iostream>

Job::Job(void (*on_done)(Job*), bool (*continue_job) (Job*))
    : on_done(on_done),
      continue_job(continue_job) { }

void Job::add_dependency(Job* dependency) {
    dependencies_left++;
    dependency->dependent_jobs.push(this);
}

void Job::complete() {
    on_done(this);
    completed = true;

    for (Job *job : dependent_jobs)
        job->dependencies_left --;
}

bool AST_GlobalContext::run_jobs() {
    bool has_uncompleted_jobs = true;
    bool completed_a_job = true;

    while (has_uncompleted_jobs && completed_a_job) {
        completed_a_job = false;
        has_uncompleted_jobs = false;

        // TODO
        for (i64 i = jobs.size - 1; i >= 0; i-- ) {
            Job *j = jobs[i];
            if (!j->completed) {
                has_uncompleted_jobs = true;

                if (j->dependencies_left == 0) {
                    completed_a_job = true;
                    // wcout << "Continuing job " << i << "\n";
                    if (j->continue_job(j)) {
                        j->complete();
                    }
                }
                else {
                    // wcout << "Job " << i << " has " << jobs[i]->dependencies_left << " dependencies\n";
                }
            }
        }
    }

    if (has_uncompleted_jobs) {
        std::wcout << "Failed to complete all jobs. Possible circular dependency\n";
        return false;
    }

    return true;
}

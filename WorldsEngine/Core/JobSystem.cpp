#include "JobSystem.hpp"
#include <iostream>
#ifdef TRACY_ENABLE
#include "tracy/Tracy.hpp"
#endif
#ifdef __linux__
#include <pthread.h>
#endif
#include <Core/Fatal.hpp>

namespace worlds {
    JobList::JobList()
        : jobs{ 256 }
        , completeCV{ nullptr }
        , completeMutex{ nullptr }
        , jobCount{ 0 } {
        completeCV = SDL_CreateCond();
        completeMutex = SDL_CreateMutex();
    }

    void JobList::begin() {
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
    }

    void JobList::addJob(Job&& job) {
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        jobs.enqueue(std::move(job));
        jobCount++;
    }

    void JobList::end() {
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
    }

    void JobList::wait() {
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        if (jobCount == 0) return;
        SDL_LockMutex(completeMutex);
        while (jobCount) {
            SDL_CondWait(completeCV, completeMutex);
        }
        SDL_UnlockMutex(completeMutex);
    }

    bool JobList::isComplete() {
        return jobCount == 0;
    }

    JobSystem::JobSystem(int numWorkers)
        : executing(true) {
        jobListsMutex.lock();
        currentJobLists = new JobList[NUM_JOB_SLOTS];

        for (int i = 0; i < numWorkers; i++) {
            auto t = std::thread(&JobSystem::worker, this, i);
#ifdef __linux__
            pthread_setname_np(t.native_handle(), "worker thread");
#endif
            workers.push_back(std::move(t));
        }

        jobListsMutex.unlock();

        do {} while (initialisedWorkerCount != numWorkers);
    }

    JobSystem::~JobSystem() {
        executing = false;

        for (int i = 0; i < NUM_JOB_SLOTS; i++) {
            SDL_DestroyCond(currentJobLists[i].completeCV);
            SDL_DestroyMutex(currentJobLists[i].completeMutex);
        }

        newJobListCV.notify_all();
        for (auto& w : workers) {
            w.join();
        }

        delete[] currentJobLists;
    }

    JobList& JobSystem::getFreeJobList() {
        return currentJobLists[getFreeJobSlot()];
    }

    void JobSystem::signalJobListAvailable() {
        newJobListCV.notify_all();
    }

    void JobSystem::completeFrameJobs() {
        for (int i = 0; i < NUM_JOB_SLOTS; i++) {
            JobList& jobList = currentJobLists[i];
            if (jobList.jobCount)
                jobList.wait();
        }
    }

    int JobSystem::getFreeJobSlot() {
        for (int i = 0; i < NUM_JOB_SLOTS; i++) {
            if (currentJobLists[i].jobCount == 0)
                return i;
        }
        fatalErr("No free job lists");
        return -1;
    }

    void JobSystem::worker(int idx) {
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        initialisedWorkerCount++;

        while (executing) {
            // Get a job from any of the job lists
            jobListsMutex.lock();
            Job currentJob(nullptr);
            JobList* pulledFromList = nullptr;

            for (int i = 0; i < NUM_JOB_SLOTS; i++) {
                JobList& jobList = currentJobLists[i];
                if (jobList.jobCount && jobList.jobs.try_dequeue(currentJob)) {
                    pulledFromList = &jobList;
                    break;
                }
            }
            jobListsMutex.unlock();

            // No jobs available :(
            if (pulledFromList == nullptr) {
                // Wait for any new job lists
                {
                    std::unique_lock<std::mutex> newJobListLck(newJobListM);
                    newJobListCV.wait(newJobListLck);
                }

                // Check if the "available job list" was actually just
                // the program terminating
                if (!executing)
                    return;
                continue;
            }

            // Run the job
            currentJob.function();
            if (currentJob.completeFunc)
                currentJob.completeFunc();

            // Decrement the job count and fire completion if necessary
            pulledFromList->jobCount--;
            if (pulledFromList->jobCount == 0) {
                SDL_LockMutex(pulledFromList->completeMutex);
                SDL_CondBroadcast(pulledFromList->completeCV);
                SDL_UnlockMutex(pulledFromList->completeMutex);
            }
        }
    }
}

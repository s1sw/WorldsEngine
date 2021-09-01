#pragma once
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <atomic>
#include <SDL_mutex.h>
#include <functional>
#include "tracy/Tracy.hpp"

namespace worlds {
    typedef std::function<void()> JobFunc;
    typedef void (*JobCompleteFuncPtr)();

    struct Job {
        Job(JobFunc function, JobCompleteFuncPtr completeFunc = nullptr)
            : function(function)
            , completeFunc(completeFunc) {
        }

        JobFunc function;
        JobCompleteFuncPtr completeFunc;
    };

    enum class QueueType {
        PerFrame,
        Background
    };
    const int NUM_JOB_SLOTS = 3;

    class JobList{
    public:
        JobList() :
            startJobCount(0),
            completedJobs(0),
            completed(true) {

        }

        void begin() {
#ifdef TRACY_ENABLE
            ZoneScoped;
#endif
            jobs = std::queue<Job>();
        }

        void addJob(Job&& job) {
#ifdef TRACY_ENABLE
            ZoneScoped;
#endif
            jobs.emplace(job);
        }

        void end() {
#ifdef TRACY_ENABLE
            ZoneScoped;
#endif
            startJobCount = (int)jobs.size();
            completedJobs = 0;
            completed = false;
        }

        void wait() {
#ifdef TRACY_ENABLE
            ZoneScoped;
#endif
            if (completed) return;
            SDL_LockMutex(completeMutex);
            while (!completed) {
                SDL_CondWait(completeCV, completeMutex);
            }
            SDL_UnlockMutex(completeMutex);
        }
    private:
        std::queue<Job> jobs;
        std::atomic<int> startJobCount;
        std::atomic<int> completedJobs;
        std::atomic<bool> completed;
        SDL_cond* completeCV;
        SDL_mutex* completeMutex;
        friend class JobSystem;
    };

    class JobSystem {
    public:
        JobSystem(int numWorkers);
        ~JobSystem();
        //void addJobs(std::queue<Job> jobQueue);
        //void addJobsAndWait(std::queue<Job> jobQueue);
        //void addJob(Job job);
        //void addJobAndWait(Job job);
        JobList& getFreeJobList();
        void signalJobListAvailable();
        void completeFrameJobs();

    private:
        int getFreeJobSlot();
        void worker(int idx);
        bool executing;
        std::mutex jobListsMutex;
        std::mutex newJobListM;
        std::condition_variable newJobListCV;

        JobList currentJobLists[NUM_JOB_SLOTS];
        std::vector<std::thread> workers;
    };
}

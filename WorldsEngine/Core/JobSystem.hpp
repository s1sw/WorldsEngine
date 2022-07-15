#pragma once
#include "concurrentqueue.h"
#include "tracy/Tracy.hpp"
#include <SDL_mutex.h>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

namespace worlds
{
    class JobSystem;
    extern JobSystem *g_jobSys;
    typedef std::function<void()> JobFunc;
    typedef void (*JobCompleteFuncPtr)();

    struct Job
    {
        Job(JobFunc function, JobCompleteFuncPtr completeFunc = nullptr)
            : function(function), completeFunc(completeFunc)
        {
        }

        JobFunc function;
        JobCompleteFuncPtr completeFunc;
    };

    enum class QueueType
    {
        PerFrame,
        Background
    };
    const int NUM_JOB_SLOTS = 3;

    class JobList
    {
      public:
        JobList();

        void begin();
        void addJob(Job &&job);
        void end();
        void wait();

        bool isComplete();

      private:
        moodycamel::ConcurrentQueue<Job> jobs;
        std::atomic<int> jobCount;
        SDL_cond *completeCV;
        SDL_mutex *completeMutex;
        friend class JobSystem;
    };

    class JobSystem
    {
      public:
        JobSystem(int numWorkers);
        ~JobSystem();
        JobList &getFreeJobList();
        void signalJobListAvailable();
        void completeFrameJobs();

      private:
        int getFreeJobSlot();
        void worker(int idx);
        bool executing;
        std::mutex jobListsMutex;
        std::mutex newJobListM;
        std::condition_variable newJobListCV;

        JobList *currentJobLists;
        std::vector<std::thread> workers;
        std::atomic<int> initialisedWorkerCount;
    };
}

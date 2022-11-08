#pragma once
#include "TaskScheduler.h"

namespace worlds
{
    extern enki::TaskScheduler g_taskSched;

    struct TasksFinished : public enki::ICompletable
    {
        std::vector<enki::Dependency> dependencies;
    };

    struct TaskDeleter : public enki::ICompletable
    {
        enki::Dependency dependency;
        bool deleteSelf = false;

        void OnDependenciesComplete(enki::TaskScheduler* ts, uint32_t threadnum) override
        {
            bool shouldDeleteSelf = deleteSelf;
            ICompletable::OnDependenciesComplete(ts, threadnum);
            delete dependency.GetDependencyTask();
            if (shouldDeleteSelf)
                delete this;
        }
    };
}
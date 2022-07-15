#pragma once
#include <cassert>
#include <chrono>

namespace worlds
{
    class TimingUtil
    {
      public:
        static std::chrono::time_point<std::chrono::high_resolution_clock> now()
        {
            return std::chrono::high_resolution_clock::now();
        }

        static double toMs(std::chrono::nanoseconds ns)
        {
            return ((double)ns.count() / 1000.0 / 1000.0);
        }
    };

    class PerfTimer
    {
        std::chrono::time_point<std::chrono::high_resolution_clock> start;
        std::chrono::time_point<std::chrono::high_resolution_clock> end;
        bool stopped = false;

      public:
        PerfTimer()
        {
            start = TimingUtil::now();
        }

        void stop()
        {
            assert(!stopped);
            stopped = true;
            end = TimingUtil::now();
        }

        double stopGetMs()
        {
            if (!stopped)
                stop();

            return TimingUtil::toMs(end - start);
        }
    };
}

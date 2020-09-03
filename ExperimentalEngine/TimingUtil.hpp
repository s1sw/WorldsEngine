#pragma once
#include <chrono>

namespace worlds {
    class TimingUtil {
    public:
        static auto now() {
            return std::chrono::high_resolution_clock::now();
        }

        static double toMs(std::chrono::nanoseconds ns) {
            return ((double)ns.count() / 1000.0 / 1000.0);
        }
    };

    class PerfTimer {
        std::chrono::steady_clock::time_point start;
        std::chrono::steady_clock::time_point end;
        bool stopped = false;
    public:
        PerfTimer() {
            start = TimingUtil::now();
        }

        void stop() {
            assert(!stopped);
            stopped = true;
            end = TimingUtil::now();
        }

        double stopGetMs() {
            if (!stopped) stop();

            return TimingUtil::toMs(end - start);
        }
    };
}
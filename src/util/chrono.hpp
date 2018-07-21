#pragma once

#include <chrono>
#include <ctime>

namespace cloth::chrono {

    using namespace std::chrono;

    using clock = std::chrono::system_clock;
    using duration = clock::duration;
    using time_point = std::chrono::time_point<clock, duration>;

    inline struct timespec to_timespec(time_point t) noexcept
    {
      long secs = duration_cast<seconds>(t.time_since_epoch()).count();
      long nsc = nanoseconds(t.time_since_epoch()).count() % 1000000000;
      return {secs, nsc};
    }

    inline time_point to_time_point(struct timespec t) noexcept
    {
      return time_point() + seconds(t.tv_sec) + nanoseconds(t.tv_nsec);
    }

}

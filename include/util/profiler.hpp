#include <base.hpp>

#define PROFILER_COUNTERS(X) \
    X(TotalBranches, u64) \
    X(MissBranches, u32) \
    X(TotalL1DAccess, u32) \
    X(L1DMisses, u32) \
    X(TotalL2DAccess, u32) \
    X(L2DMisses, u32)

namespace prof {
    class Profiler {
    public:
        Profiler();
        ~Profiler();
        static void __always_inline initializeProfiler();
        static void __always_inline deinitializeProfiler();

        void startProfiling();
        void stopProfiling();

        void logResults();

        u64 getCycleCounter() const { return mEndCycleCount - mStartCycleCount; }

#define PROFILER_DECLARE_GETTER(Name, Type) \
        Type get##Name() const { return mEnd##Name - mStart##Name; }

        PROFILER_COUNTERS(PROFILER_DECLARE_GETTER)

    private:
        void __always_inline fetchCountersStart();
        void __always_inline fetchCountersEnd();

        u64 mEndCycleCount = 0;
        u64 mStartCycleCount = 0;

#define PROFILER_DECLARE_MEMBER(Name, Type) \
        Type mEnd##Name = 0; \
        Type mStart##Name = 0;

        PROFILER_COUNTERS(PROFILER_DECLARE_MEMBER)
    };
}

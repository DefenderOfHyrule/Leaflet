#include <base.hpp>

#define PROFILER_COUNTERS(X) \
    X(TotalBranches,  u64) \
    X(MissBranches,   u64) \
    X(TotalL1DAccess, u64) \
    X(L1DMisses,      u64) \
    X(TotalL2DAccess, u64) \
    X(L2DMisses,      u64)

namespace prof {
    class Profiler {
    public:
        Profiler();
        ~Profiler();

        static void clearCounters();

        static void __always_inline initializeProfiler();
        static void __always_inline deinitializeProfiler();

        static void startProfiling();
        void stopProfiling();

        void logResults() const;

        u64 getCycleCounter() const { return mCycleCount; }

#define PROFILER_DECLARE_GETTER(Name, Type) \
        Type get##Name() const { return m##Name; }

        PROFILER_COUNTERS(PROFILER_DECLARE_GETTER)

        void fetchCounters();
    private:

        u64 mCycleCount = 0;

#define PROFILER_DECLARE_MEMBER(Name, Type) \
        Type m##Name = 0;

        PROFILER_COUNTERS(PROFILER_DECLARE_MEMBER)
    };
}

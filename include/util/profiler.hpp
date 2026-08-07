#include <base.hpp>

#define PROFILER_COUNTERS(X) \
    X(0, TotalBranches,  u64, PMU_BR_PRED) \
    X(1, MissBranches,   u64, PMU_BR_MIS_PRED) \
    X(2, TotalL1DAccess, u64, PMU_L1D_CACHE) \
    X(3, L1DMisses,      u64, PMU_L1D_CACHE_REFILL) \
    X(4, TotalL2DAccess, u64, PMU_L2D_CACHE) \
    X(5, L2DMisses,      u64, PMU_L2D_CACHE_REFILL)

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


        static u64 readCycleCounter();
#define PROFILER_DECLARE_READER(Index, Name, Type, Event) \
        Type read##Name() const;

        PROFILER_COUNTERS(PROFILER_DECLARE_READER);

        u64 __always_inline getCycleCounter() const { return mCycleCount; }
#define PROFILER_DECLARE_GETTER(Index, Name, Type, Event) \
        Type get##Name() const { return m##Name; }

        PROFILER_COUNTERS(PROFILER_DECLARE_GETTER)

        void fetchCounters();
    private:

        u64 mCycleCount = 0;

#define PROFILER_DECLARE_MEMBER(Index, Name, Type, Event) \
        Type m##Name = 0;

        PROFILER_COUNTERS(PROFILER_DECLARE_MEMBER)
    };
}

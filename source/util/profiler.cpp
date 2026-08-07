#include "util/profiler.hpp"

#include "error.hpp"
#include "util/PMU.h"

#define PROFILER_COUNTERS(X) \
    X(0, TotalBranches, PMU_BR_PRED) \
    X(1, MissBranches, PMU_BR_MIS_PRED) \
    X(2, TotalL1DAccess, PMU_L1D_CACHE) \
    X(3, L1DMisses, PMU_L1D_CACHE_REFILL) \
    X(4, TotalL2DAccess, PMU_L2D_CACHE) \
    X(5, L2DMisses, PMU_L2D_CACHE_REFILL)

namespace prof {
    Profiler::Profiler() {
        this->clearCounters();
        this->initializeProfiler();
    }

    Profiler::~Profiler() {
        this->deinitializeProfiler();
    }

    void Profiler::clearCounters() {
        /* Set clear bits for all 6 event counters and the cycle counter */
        SET_PMCNTENCLR_EL0(PMCNTENCLR_EL0_P0 | PMCNTENCLR_EL0_P1 | PMCNTENCLR_EL0_P2 | PMCNTENCLR_EL0_P3 |
                           PMCNTENCLR_EL0_P4 | PMCNTENCLR_EL0_P5 | PMCNTENCLR_EL0_C);
    }

    void Profiler::initializeProfiler() {
        /* Initialize all counters, and reset event and cycle counters, and set long cycle */
        SET_PMCR_EL0(PMCR_EL0_E | PMCR_EL0_P | PMCR_EL0_C | PMCR_EL0_LC);

#define CONFIGURE_COUNTER(counterIndex, Name, Event) \
        SET_PMEVTYPERN_EL0(PMEVTYPER##counterIndex##_EL0, Event);

        PROFILER_COUNTERS(CONFIGURE_COUNTER)
    }

    void Profiler::deinitializeProfiler() {
        /* Set clear bits for event and clock counters */
        SET_PMCNTENCLR_EL0(PMCNTENCLR_EL0_P0 | PMCNTENCLR_EL0_P1 | PMCNTENCLR_EL0_P2 | PMCNTENCLR_EL0_P3 |
                           PMCNTENCLR_EL0_P4 | PMCNTENCLR_EL0_P5 | PMCNTENCLR_EL0_C);
    }

    void Profiler::startProfiling() {
        DATA_SYNC_BARRIER();
        INSTRUCTION_BARRIER();
        /* Set enable bits for event and clock counters */
        SET_PMCNTENSET_EL0(PMCNTENSET_EL0_P0 | PMCNTENSET_EL0_P1 | PMCNTENSET_EL0_P2 | PMCNTENSET_EL0_P3 |
                           PMCNTENSET_EL0_P4 | PMCNTENSET_EL0_P5 | PMCNTENCLR_EL0_C);
        INSTRUCTION_BARRIER();
        DATA_SYNC_BARRIER();
    }

    void Profiler::stopProfiling() {
        DATA_SYNC_BARRIER();
        INSTRUCTION_BARRIER();
        this->fetchCounters();
        // Probably unneeded, but whatever.
        this->logResults();
        INSTRUCTION_BARRIER();
        DATA_SYNC_BARRIER();
    }

    void Profiler::logResults() const {
        LOG_DEBUG("%s: %llu", "Cycle count", getCycleCounter());

#define PMU_LOG_EVENTS(value, Name, var) \
    LOG_DEBUG("%s: %llu", #Name, get##Name())

        PROFILER_COUNTERS(PMU_LOG_EVENTS);
    }

    void Profiler::fetchCounters() {
        /* Get cycle count */
        u64 CycleCount;
        GET_PMCCNTR_EL0(CycleCount);

        /* Save to variables */
#define READ_COUNTER_START(counterIndex, Name, Event) \
        u64 temp##Name; \
        GET_PMEVCNTRN_EL0(PMEVCNTR##counterIndex##_EL0, temp##Name); \
        m##Name += temp##Name - m##Name;

        PROFILER_COUNTERS(READ_COUNTER_START)

        mCycleCount += CycleCount;
    }
}
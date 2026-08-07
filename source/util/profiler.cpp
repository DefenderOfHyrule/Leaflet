#include "util/profiler.hpp"

#include "error.hpp"
#include "util/PMU.h"

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

#define CONFIGURE_COUNTER(Index, Name, Type, Event) \
        SET_PMEVTYPERN_EL0(PMEVTYPER##Index##_EL0, Event);

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

#define PMU_LOG_EVENTS(Index, Name, Type, Event) \
    LOG_DEBUG("%s: %llu", #Name, get##Name())

        PROFILER_COUNTERS(PMU_LOG_EVENTS);
    }

    void Profiler::fetchCounters() {
        /* Get cycle count */
        // u64 CycleCount;
        // GET_PMCCNTR_EL0(CycleCount);

        /* Save to variables */
#define READ_COUNTER_START(Index, Name, Type, Event) \
        Type temp##Name; \
        GET_PMEVCNTRN_EL0(PMEVCNTR##Index##_EL0, temp##Name); \
        m##Name += temp##Name - m##Name;

        PROFILER_COUNTERS(READ_COUNTER_START)

        mCycleCount += this->readCycleCounter();
    }

    u64 Profiler::readCycleCounter() {
        u64 tempCycleCount;
        GET_PMCCNTR_EL0(tempCycleCount);
        return tempCycleCount;
    }

#define PROFILER_DEFINE_READER(Index, Name, Type, Event) \
    Type Profiler::read##Name() const { Type temp##Name; GET_PMEVCNTRN_EL0(PMEVCNTR##Index##_EL0, temp##Name); return temp##Name; }

    PROFILER_COUNTERS(PROFILER_DEFINE_READER);
}
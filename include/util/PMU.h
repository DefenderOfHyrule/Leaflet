/* Armv8 Common architectural and microarchitectural events. | 0x0000-0x003F */
#define PMU_SW_INCR                0x0000UL // Instruction architecturally executed, Condition code check pass, software increment
#define PMU_L1I_CACHE_REFILL       0x0001UL // Level 1 instruction cache refill
#define PMU_L1I_TLB_REFILL         0x0002UL // Attributable Level 1 instruction TLB refill
#define PMU_L1D_CACHE_REFILL       0x0003UL // Level 1 data cache refill
#define PMU_L1D_CACHE              0x0004UL // Level 1 data cache access
#define PMU_L1D_TLB_REFILL         0x0005UL // Attributable Level 1 data TLB refill
#define PMU_LD_RETIRED             0x0006UL // Instruction architecturally executed, Condition code check pass, load
#define PMU_ST_RETIRED             0x0007UL // Instruction architecturally executed, Condition code check pass, store
#define PMU_INST_RETIRED           0x0008UL // Instruction architecturally executed
#define PMU_EXC_TAKEN              0x0009UL // Exception taken
#define PMU_EXC_RETURN             0x000AUL // Instruction architecturally executed, Condition code check pass, exception return
#define PMU_CID_WRITE_RETIRED      0x000BUL // Instruction architecturally executed, Condition code check pass, write to
#define PMU_PC_WRITE_RETIRED       0x000CUL // Instruction architecturally executed, Condition code check pass, software
#define PMU_BR_IMMED_RETIRED       0x000DUL // Instruction architecturally executed, immediate branch
#define PMU_BR_RETURN_RETIRED      0x000EUL // Instruction architecturally executed, Condition code check pass,
#define PMU_UNALIGNED_LDST_RETIRED 0x000FUL // Instruction architecturally executed, Condition code check pass,
#define PMU_BR_MIS_PRED            0x0010UL // Mispredicted or not predicted branch Speculatively executed
#define PMU_CPU_CYCLES             0x0011UL // Cycle
#define PMU_BR_PRED                0x0012UL // Predictable branch Speculatively executed
#define PMU_MEM_ACCESS             0x0013UL // Data memory access
#define PMU_L1I_CACHE              0x0014UL // Attributable Level 1 instruction cache access
#define PMU_L1D_CACHE_WB           0x0015UL // Attributable Level 1 data cache write-back
#define PMU_L2D_CACHE              0x0016UL // Level 2 data cache access
#define PMU_L2D_CACHE_REFILL       0x0017UL // Level 2 data cache refill
#define PMU_L2D_CACHE_WB           0x0018UL // Attributable Level 2 data cache write-back
#define PMU_BUS_ACCESS             0x0019UL // Attributable Bus access
#define PMU_MEMORY_ERROR           0x001AUL // Local memory error
#define PMU_INST_SPEC              0x001BUL // Operation Speculatively executed
#define PMU_TTBR_WRITE_RETIRED     0x001CUL // Instruction architecturally executed, Condition code check pass, write to
#define PMU_BUS_CYCLES             0x001DUL // Bus cycle
#define PMU_CHAIN                  0x001EUL //
#define PMU_L1D_CACHE_ALLOCATE     0x001FUL // Attributable Level 1 data cache allocation without refill
#define PMU_L2D_CACHE_ALLOCATE     0x0020UL // Attributable Level 2 data cache allocation without refill
#define PMU_BR_RETIRED             0x0021UL // Instruction architecturally executed, branch
#define PMU_BR_MIS_PRED_RETIRED    0x0022UL // Instruction architecturally executed, mispredicted branch
#define PMU_STALL_FRONTEND         0x0023UL // No operation issued due to the frontend
#define PMU_STALL_BACKEND          0x0024UL // No operation issued due to the backend
#define PMU_L1D_TLB                0x0025UL // Attributable Level 1 data or unified TLB access
#define PMU_L1I_TLB                0x0026UL // Attributable Level 1 instruction TLB access
#define PMU_L2I_CACHE              0x0027UL // Attributable Level 2 instruction cache access
#define PMU_L2I_CACHE_REFILL       0x0028UL // Attributable Level 2 instruction cache refill
#define PMU_L3D_CACHE_ALLOCATE     0x0029UL // Attributable Level 3 data cache allocation without refill
#define PMU_L3D_CACHE_REFILL       0x002AUL // Attributable Level 3 data cache refill
#define PMU_L3D_CACHE              0x002BUL // Attributable Level 3 data cache access
#define PMU_L3D_CACHE_WB           0x002CUL // Attributable Level 3 data cache write-back
#define PMU_L2D_TLB_REFILL         0x002DUL // Attributable Level 2 data TLB refill
#define PMU_L2I_TLB_REFILL         0x002EUL // Attributable Level 2 instruction TLB refill
#define PMU_L2D_TLB                0x002FUL // Attributable Level 2 data or unified TLB access
#define PMU_L2I_TLB                0x0030UL // Attributable Level 2 instruction TLB access
#define PMU_REMOTE_ACCESS          0x0031UL // Access to another socket in a multi-socket system
#define PMU_LL_CACHE               0x0032UL // Last Level cache access
#define PMU_LL_CACHE_MISS          0x0033UL // Last Level cache miss
#define PMU_DTLB_WALK              0x0034UL // Access to data TLB causes a translation table walk
#define PMU_ITLB_WALK              0x0035UL // Access to instruction TLB that causes a translation table walk
#define PMU_LL_CACHE_RD            0x0036UL // Attributable Last level cache memory read
#define PMU_LL_CACHE_MISS_RD       0x0037UL // Last level cache miss, read
#define PMU_REMOTE_ACCESS_RD       0x0038UL // Access to another socket in a multi-socket system, read
#define PMU_L1D_CACHE_LMISS_RD     0x0039UL // Level 1 data cache long-latency read miss
#define PMU_OP_RETIRED             0x003AUL // Micro-operation architecturally executed
#define PMU_OP_SPEC                0x003BUL // Micro-operation Speculatively executed
#define PMU_STALL                  0x003CUL // No operation sent for execution
#define PMU_STALL_SLOT_BACKEND     0x003DUL // No operation sent for execution on a Slot due to the backend
#define PMU_STALL_SLOT_FRONTEND    0x003EUL // No operation sent for execution on a Slot due to the frontend
#define PMU_STALL_SLOT             0x003FUL // No operation sent for execution on a Slot

/* Register bitset definitions */
#define BIT(n) (1UL << (n))

/* Performance Monitors Control Register (PMCR_EL0) bits */
#define PMCR_EL0_E       BIT(00) // Enable all counters
#define PMCR_EL0_P       BIT(01) // Event counter reset
#define PMCR_EL0_C       BIT(02) // Cycle counter reset
#define PMCR_EL0_D       BIT(03) // Clock divider, (count once per cycle, or once per 64 cycles)
#define PMCR_EL0_X       BIT(04) // Enable export of events in a PMU event export bus
#define PMCR_EL0_RES0    BIT(05) // Reserved
#define PMCR_EL0_LC      BIT(06) // Long cycle counter enabled
#define PMCR_EL0_RES1    BIT(07) // Seserved
#define PMCR_EL0_RES2    BIT(08) // Reserved
#define PMCR_EL0_RES3    BIT(09) // Reserved
#define PMCR_EL0_RES4    BIT(10) // Reserved
#define PMCR_EL0_N0      BIT(11) // Number of total event counters; bit 0
#define PMCR_EL0_N1      BIT(12) // Number of total event counters; bit 1
#define PMCR_EL0_N2      BIT(13) // Number of total event counters; bit 2
#define PMCR_EL0_N3      BIT(14) // Number of total event counters; bit 3
#define PMCR_EL0_N4      BIT(15) // Number of total event counters; bit 4
#define PMCR_EL0_IDCODE0 BIT(16) // !Deprecated! Identification code; bit 0
#define PMCR_EL0_IDCODE1 BIT(17) // !Deprecated! Identification code; bit 1
#define PMCR_EL0_IDCODE2 BIT(18) // !Deprecated! Identification code; bit 2
#define PMCR_EL0_IDCODE3 BIT(19) // !Deprecated! Identification code; bit 3
#define PMCR_EL0_IDCODE4 BIT(20) // !Deprecated! Identification code; bit 4
#define PMCR_EL0_IDCODE5 BIT(21) // !Deprecated! Identification code; bit 5
#define PMCR_EL0_IDCODE6 BIT(22) // !Deprecated! Identification code; bit 6
#define PMCR_EL0_IDCODE7 BIT(23) // !Deprecated! Identification code; bit 7
#define PMCR_EL0_IMP0    BIT(24) // !Deprecated! Implementer code; bit 0
#define PMCR_EL0_IMP1    BIT(25) // !Deprecated! Implementer code; bit 1
#define PMCR_EL0_IMP2    BIT(26) // !Deprecated! Implementer code; bit 2
#define PMCR_EL0_IMP3    BIT(27) // !Deprecated! Implementer code; bit 3
#define PMCR_EL0_IMP4    BIT(28) // !Deprecated! Implementer code; bit 4
#define PMCR_EL0_IMP5    BIT(29) // !Deprecated! Implementer code; bit 5
#define PMCR_EL0_IMP6    BIT(30) // !Deprecated! Implementer code; bit 6
#define PMCR_EL0_IMP7    BIT(31) // !Deprecated! Implementer code; bit 7

/* Performance Monitors Count Enable Set register (PMCNTENSET_EL0) bits */
#define PMCNTENSET_EL0_P0    BIT(00) // Event counter enable bit for PMEVCNTR00_EL0
#define PMCNTENSET_EL0_P1    BIT(01) // Event counter enable bit for PMEVCNTR01_EL0
#define PMCNTENSET_EL0_P2    BIT(02) // Event counter enable bit for PMEVCNTR02_EL0
#define PMCNTENSET_EL0_P3    BIT(03) // Event counter enable bit for PMEVCNTR03_EL0
#define PMCNTENSET_EL0_P4    BIT(04) // Event counter enable bit for PMEVCNTR04_EL0
#define PMCNTENSET_EL0_P5    BIT(05) // Event counter enable bit for PMEVCNTR05_EL0
#define PMCNTENSET_EL0_RES0  BIT(06) // Reserved
#define PMCNTENSET_EL0_RES1  BIT(07) // Reserved.
#define PMCNTENSET_EL0_RES2  BIT(08) // Reserved.
#define PMCNTENSET_EL0_RES3  BIT(09) // Reserved.
#define PMCNTENSET_EL0_RES4  BIT(10) // Reserved.
#define PMCNTENSET_EL0_RES5  BIT(11) // Reserved.
#define PMCNTENSET_EL0_RES6  BIT(12) // Reserved.
#define PMCNTENSET_EL0_RES7  BIT(13) // Reserved.
#define PMCNTENSET_EL0_RES8  BIT(14) // Reserved.
#define PMCNTENSET_EL0_RES9  BIT(15) // Reserved.
#define PMCNTENSET_EL0_RES10 BIT(16) // Reserved.
#define PMCNTENSET_EL0_RES11 BIT(17) // Reserved.
#define PMCNTENSET_EL0_RES12 BIT(18) // Reserved.
#define PMCNTENSET_EL0_RES13 BIT(19) // Reserved.
#define PMCNTENSET_EL0_RES14 BIT(20) // Reserved.
#define PMCNTENSET_EL0_RES15 BIT(21) // Reserved.
#define PMCNTENSET_EL0_RES16 BIT(22) // Reserved.
#define PMCNTENSET_EL0_RES17 BIT(23) // Reserved.
#define PMCNTENSET_EL0_RES18 BIT(24) // Reserved.
#define PMCNTENSET_EL0_RES19 BIT(25) // Reserved.
#define PMCNTENSET_EL0_RES20 BIT(26) // Reserved.
#define PMCNTENSET_EL0_RES21 BIT(27) // Reserved.
#define PMCNTENSET_EL0_RES22 BIT(28) // Reserved.
#define PMCNTENSET_EL0_RES23 BIT(29) // Reserved.
#define PMCNTENSET_EL0_RES24 BIT(30) // Reserved.
#define PMCNTENSET_EL0_C     BIT(31) // Cycle counter enable bit for PMCCNTR_EL0

/* Performance Monitors Count Enable Clear register (PMCNTENCLR_EL0) bits */
#define PMCNTENCLR_EL0_P0    BIT(00) // Event counter disable bit for PMEVCNTR00_EL0
#define PMCNTENCLR_EL0_P1    BIT(01) // Event counter disable bit for PMEVCNTR01_EL0
#define PMCNTENCLR_EL0_P2    BIT(02) // Event counter disable bit for PMEVCNTR02_EL0
#define PMCNTENCLR_EL0_P3    BIT(03) // Event counter disable bit for PMEVCNTR03_EL0
#define PMCNTENCLR_EL0_P4    BIT(04) // Event counter disable bit for PMEVCNTR04_EL0
#define PMCNTENCLR_EL0_P5    BIT(05) // Event counter disable bit for PMEVCNTR05_EL0
#define PMCNTENCLR_EL0_RES0  BIT(06) // Reserved
#define PMCNTENCLR_EL0_RES1  BIT(07) // Reserved
#define PMCNTENCLR_EL0_RES2  BIT(08) // Reserved
#define PMCNTENCLR_EL0_RES3  BIT(09) // Reserved
#define PMCNTENCLR_EL0_RES4  BIT(10) // Reserved
#define PMCNTENCLR_EL0_RES5  BIT(11) // Reserved
#define PMCNTENCLR_EL0_RES6  BIT(12) // Reserved
#define PMCNTENCLR_EL0_RES7  BIT(13) // Reserved
#define PMCNTENCLR_EL0_RES8  BIT(14) // Reserved
#define PMCNTENCLR_EL0_RES9  BIT(15) // Reserved
#define PMCNTENCLR_EL0_RES10 BIT(16) // Reserved
#define PMCNTENCLR_EL0_RES11 BIT(17) // Reserved
#define PMCNTENCLR_EL0_RES12 BIT(18) // Reserved
#define PMCNTENCLR_EL0_RES13 BIT(19) // Reserved
#define PMCNTENCLR_EL0_RES14 BIT(20) // Reserved
#define PMCNTENCLR_EL0_RES15 BIT(21) // Reserved
#define PMCNTENCLR_EL0_RES16 BIT(22) // Reserved
#define PMCNTENCLR_EL0_RES17 BIT(23) // Reserved
#define PMCNTENCLR_EL0_RES18 BIT(24) // Reserved
#define PMCNTENCLR_EL0_RES19 BIT(25) // Reserved
#define PMCNTENCLR_EL0_RES20 BIT(26) // Reserved
#define PMCNTENCLR_EL0_RES21 BIT(27) // Reserved
#define PMCNTENCLR_EL0_RES22 BIT(28) // Reserved
#define PMCNTENCLR_EL0_RES23 BIT(29) // Reserved
#define PMCNTENCLR_EL0_RES24 BIT(30) // Reserved
#define PMCNTENCLR_EL0_C     BIT(31) // Cycle counter disable bit for PMCCNTR_EL0

/* Performance Monitors Overflow Flag Status Clear Register (PMOVSCLR_EL0) bits */
#define PMOVSCLR_EL0_P0    BIT(00) // Event counter overflow clear bit for PMEVCNTR0_EL0
#define PMOVSCLR_EL0_P1    BIT(01) // Event counter overflow clear bit for PMEVCNTR1_EL0
#define PMOVSCLR_EL0_P2    BIT(02) // Event counter overflow clear bit for PMEVCNTR2_EL0
#define PMOVSCLR_EL0_P3    BIT(03) // Event counter overflow clear bit for PMEVCNTR3_EL0
#define PMOVSCLR_EL0_P4    BIT(04) // Event counter overflow clear bit for PMEVCNTR4_EL0
#define PMOVSCLR_EL0_RES0  BIT(05) // Event counter overflow clear bit for PMEVCNTR5_EL0
#define PMOVSCLR_EL0_RES1  BIT(06) // Reserved
#define PMOVSCLR_EL0_RES2  BIT(07) // Reserved
#define PMOVSCLR_EL0_RES3  BIT(08) // Reserved
#define PMOVSCLR_EL0_RES4  BIT(09) // Reserved
#define PMOVSCLR_EL0_RES5  BIT(10) // Reserved
#define PMOVSCLR_EL0_RES6  BIT(11) // Reserved
#define PMOVSCLR_EL0_RES7  BIT(12) // Reserved
#define PMOVSCLR_EL0_RES8  BIT(13) // Reserved
#define PMOVSCLR_EL0_RES9  BIT(14) // Reserved
#define PMOVSCLR_EL0_RES10 BIT(15) // Reserved
#define PMOVSCLR_EL0_RES11 BIT(16) // Reserved
#define PMOVSCLR_EL0_RES12 BIT(17) // Reserved
#define PMOVSCLR_EL0_RES13 BIT(18) // Reserved
#define PMOVSCLR_EL0_RES14 BIT(19) // Reserved
#define PMOVSCLR_EL0_RES15 BIT(20) // Reserved
#define PMOVSCLR_EL0_RES16 BIT(21) // Reserved
#define PMOVSCLR_EL0_RES17 BIT(22) // Reserved
#define PMOVSCLR_EL0_RES18 BIT(23) // Reserved
#define PMOVSCLR_EL0_RES19 BIT(24) // Reserved
#define PMOVSCLR_EL0_RES20 BIT(25) // Reserved
#define PMOVSCLR_EL0_RES21 BIT(26) // Reserved
#define PMOVSCLR_EL0_RES22 BIT(27) // Reserved
#define PMOVSCLR_EL0_RES23 BIT(28) // Reserved
#define PMOVSCLR_EL0_RES24 BIT(29) // Reserved
#define PMOVSCLR_EL0_RES25 BIT(30) // Reserved
#define PMOVSCLR_EL0_C     BIT(31) // Cycle counter overflow clear bit

/* Performance Monitors Software Increment register (PMSWINC_EL0) bits */
#define PMSWINC_EL0_P0    BIT(00) // Event counter software increment bit for PMEVCNTR0_EL0
#define PMSWINC_EL0_P1    BIT(01) // Event counter software increment bit for PMEVCNTR1_EL0
#define PMSWINC_EL0_P2    BIT(02) // Event counter software increment bit for PMEVCNTR2_EL0
#define PMSWINC_EL0_P3    BIT(03) // Event counter software increment bit for PMEVCNTR3_EL0
#define PMSWINC_EL0_P4    BIT(04) // Event counter software increment bit for PMEVCNTR4_EL0
#define PMSWINC_EL0_RES0  BIT(05) // Event counter software increment bit for PMEVCNTR5_EL0
#define PMSWINC_EL0_RES1  BIT(06) // Reserved
#define PMSWINC_EL0_RES2  BIT(07) // Reserved
#define PMSWINC_EL0_RES3  BIT(08) // Reserved
#define PMSWINC_EL0_RES4  BIT(09) // Reserved
#define PMSWINC_EL0_RES5  BIT(10) // Reserved
#define PMSWINC_EL0_RES6  BIT(11) // Reserved
#define PMSWINC_EL0_RES7  BIT(12) // Reserved
#define PMSWINC_EL0_RES8  BIT(13) // Reserved
#define PMSWINC_EL0_RES9  BIT(14) // Reserved
#define PMSWINC_EL0_RES10 BIT(15) // Reserved
#define PMSWINC_EL0_RES11 BIT(16) // Reserved
#define PMSWINC_EL0_RES12 BIT(17) // Reserved
#define PMSWINC_EL0_RES13 BIT(18) // Reserved
#define PMSWINC_EL0_RES14 BIT(19) // Reserved
#define PMSWINC_EL0_RES15 BIT(20) // Reserved
#define PMSWINC_EL0_RES16 BIT(21) // Reserved
#define PMSWINC_EL0_RES17 BIT(22) // Reserved
#define PMSWINC_EL0_RES18 BIT(23) // Reserved
#define PMSWINC_EL0_RES19 BIT(24) // Reserved
#define PMSWINC_EL0_RES20 BIT(25) // Reserved
#define PMSWINC_EL0_RES21 BIT(26) // Reserved
#define PMSWINC_EL0_RES22 BIT(27) // Reserved
#define PMSWINC_EL0_RES23 BIT(28) // Reserved
#define PMSWINC_EL0_RES24 BIT(29) // Reserved
#define PMSWINC_EL0_RES25 BIT(30) // Reserved
#define PMSWINC_EL0_RES26 BIT(31) // Reserved

/* Performance Monitors Event Counter Selection Register (PMSELR_EL0) bits */
#define PMSELR_EL0_SEL0  BIT(00) // Selects Nth event counter, where N is a u5 0:4; bit 0
#define PMSELR_EL0_SEL1  BIT(01) // Selects Nth event counter, where N is a u5 0:4; bit 1
#define PMSELR_EL0_SEL2  BIT(02) // Selects Nth event counter, where N is a u5 0:4; bit 2
#define PMSELR_EL0_SEL3  BIT(03) // Selects Nth event counter, where N is a u5 0:4; bit 3
#define PMSELR_EL0_SEL4  BIT(04) // Selects Nth event counter, where N is a u5 0:4; bit 4
#define PMSELR_EL0_RES0  BIT(05) // Reserved
#define PMSELR_EL0_RES1  BIT(06) // Reserved
#define PMSELR_EL0_RES2  BIT(07) // Reserved
#define PMSELR_EL0_RES3  BIT(08) // Reserved
#define PMSELR_EL0_RES4  BIT(09) // Reserved
#define PMSELR_EL0_RES5  BIT(10) // Reserved
#define PMSELR_EL0_RES6  BIT(11) // Reserved
#define PMSELR_EL0_RES7  BIT(12) // Reserved
#define PMSELR_EL0_RES8  BIT(13) // Reserved
#define PMSELR_EL0_RES9  BIT(14) // Reserved
#define PMSELR_EL0_RES10 BIT(15) // Reserved
#define PMSELR_EL0_RES11 BIT(16) // Reserved
#define PMSELR_EL0_RES12 BIT(17) // Reserved
#define PMSELR_EL0_RES13 BIT(18) // Reserved
#define PMSELR_EL0_RES14 BIT(19) // Reserved
#define PMSELR_EL0_RES15 BIT(20) // Reserved
#define PMSELR_EL0_RES16 BIT(21) // Reserved
#define PMSELR_EL0_RES17 BIT(22) // Reserved
#define PMSELR_EL0_RES18 BIT(23) // Reserved
#define PMSELR_EL0_RES19 BIT(24) // Reserved
#define PMSELR_EL0_RES20 BIT(25) // Reserved
#define PMSELR_EL0_RES21 BIT(26) // Reserved
#define PMSELR_EL0_RES22 BIT(27) // Reserved
#define PMSELR_EL0_RES23 BIT(28) // Reserved
#define PMSELR_EL0_RES24 BIT(29) // Reserved
#define PMSELR_EL0_RES25 BIT(30) // Reserved
#define PMSELR_EL0_RES26 BIT(31) // Reserved

/* Performance Monitors Common Event Identification register 0 (PMCEID0_EL0) bits */
#define PMCEID0_EL0_ID0  BIT(00) // Common event SW_INCR                (0x0000) enabled
#define PMCEID0_EL0_ID1  BIT(01) // Common event L1I_CACHE_REFILL       (0x0001) enabled
#define PMCEID0_EL0_ID2  BIT(02) // Common event L1I_TLB_REFILL         (0x0002) enabled
#define PMCEID0_EL0_ID3  BIT(03) // Common event L1D_CACHE_REFILL       (0x0003) enabled
#define PMCEID0_EL0_ID4  BIT(04) // Common event L1D_CACHE              (0x0004) enabled
#define PMCEID0_EL0_ID5  BIT(05) // Common event L1D_TLB_REFILL         (0x0005) enabled
#define PMCEID0_EL0_ID6  BIT(06) // Common event LD_RETIRED             (0x0006) enabled
#define PMCEID0_EL0_ID7  BIT(07) // Common event ST_RETIRED             (0x0007) enabled
#define PMCEID0_EL0_ID8  BIT(08) // Common event INST_RETIRED           (0x0008) enabled
#define PMCEID0_EL0_ID9  BIT(09) // Common event EXC_TAKEN              (0x0009) enabled
#define PMCEID0_EL0_ID10 BIT(10) // Common event EXC_RETURN             (0x000A) enabled
#define PMCEID0_EL0_ID11 BIT(11) // Common event CID_WRITE_RETIRED      (0x000B) enabled
#define PMCEID0_EL0_ID12 BIT(12) // Common event PC_WRITE_RETIRED       (0x000C) enabled
#define PMCEID0_EL0_ID13 BIT(13) // Common event BR_IMMED_RETIRED       (0x000D) enabled
#define PMCEID0_EL0_ID14 BIT(14) // Common event BR_RETURN_RETIRED      (0x000E) enabled
#define PMCEID0_EL0_ID15 BIT(15) // Common event UNALIGNED_LDST_RETIRED (0x000F) enabled
#define PMCEID0_EL0_ID16 BIT(16) // Common event BR_MIS_PRED            (0x0010) enabled
#define PMCEID0_EL0_ID17 BIT(17) // Common event CPU_CYCLES             (0x0011) enabled
#define PMCEID0_EL0_ID18 BIT(18) // Common event BR_PRED                (0x0012) enabled
#define PMCEID0_EL0_ID19 BIT(19) // Common event MEM_ACCESS             (0x0013) enabled
#define PMCEID0_EL0_ID20 BIT(20) // Common event L1I_CACHE              (0x0014) enabled
#define PMCEID0_EL0_ID21 BIT(21) // Common event L1D_CACHE_WB           (0x0015) enabled
#define PMCEID0_EL0_ID22 BIT(22) // Common event L2D_CACHE              (0x0016) enabled
#define PMCEID0_EL0_ID23 BIT(23) // Common event L2D_CACHE_REFILL       (0x0017) enabled
#define PMCEID0_EL0_ID24 BIT(24) // Common event L2D_CACHE_WB           (0x0018) enabled
#define PMCEID0_EL0_ID25 BIT(25) // Common event BUS_ACCESS             (0x0019) enabled
#define PMCEID0_EL0_ID26 BIT(26) // Common event MEMORY_ERROR           (0x001A) enabled
#define PMCEID0_EL0_ID27 BIT(27) // Common event INST_SPEC              (0x001B) enabled
#define PMCEID0_EL0_ID28 BIT(28) // Common event TTBR_WRITE_RETIRED     (0x001C) enabled
#define PMCEID0_EL0_ID29 BIT(29) // Common event BUS_CYCLES             (0x001D) enabled
#define PMCEID0_EL0_ID30 BIT(30) // Common event CHAIN                  (0x001E) enabled
#define PMCEID0_EL0_ID31 BIT(31) // Common event L1D_CACHE_ALLOCATE     (0x001F) enabled

/* Performance Monitors Common Event Identification register 1 (PMCEID1_EL0) bits */
#define PMCEID1_EL0_ID0  BIT(00) // Common event L2D_CACHE_ALLOCATE     (0x0020) enabled
#define PMCEID1_EL0_ID1  BIT(01) // Common event BR_RETIRED             (0x0021) enabled
#define PMCEID1_EL0_ID2  BIT(02) // Common event BR_MIS_PRED_RETIRED    (0x0022) enabled
#define PMCEID1_EL0_ID3  BIT(03) // Common event STALL_FRONTEND         (0x0023) enabled
#define PMCEID1_EL0_ID4  BIT(04) // Common event STALL_BACKEND          (0x0024) enabled
#define PMCEID1_EL0_ID5  BIT(05) // Common event L1D_TLB                (0x0025) enabled
#define PMCEID1_EL0_ID6  BIT(06) // Common event L1I_TLB                (0x0026) enabled
#define PMCEID1_EL0_ID7  BIT(07) // Common event L2I_CACHE              (0x0027) enabled
#define PMCEID1_EL0_ID8  BIT(08) // Common event L2I_CACHE_REFILL       (0x0028) enabled
#define PMCEID1_EL0_ID9  BIT(09) // Common event L3D_CACHE_ALLOCATE     (0x0029) enabled
#define PMCEID1_EL0_ID10 BIT(10) // Common event L3D_CACHE_REFILL       (0x002A) enabled
#define PMCEID1_EL0_ID11 BIT(11) // Common event L3D_CACHE              (0x002B) enabled
#define PMCEID1_EL0_ID12 BIT(12) // Common event L3D_CACHE_WB           (0x002C) enabled
#define PMCEID1_EL0_ID13 BIT(13) // Common event L2D_TLB_REFILL         (0x002D) enabled
#define PMCEID1_EL0_ID14 BIT(14) // Common event L2I_TLB_REFILL         (0x002E) enabled
#define PMCEID1_EL0_ID15 BIT(15) // Common event L2D_TLB                (0x002F) enabled
#define PMCEID1_EL0_ID16 BIT(16) // Common event L2I_TLB                (0x0030) enabled
#define PMCEID1_EL0_ID17 BIT(17) // Common event REMOTE_ACCESS          (0x0031) enabled
#define PMCEID1_EL0_ID18 BIT(18) // Common event LL_CACHE               (0x0032) enabled
#define PMCEID1_EL0_ID19 BIT(19) // Common event LL_CACHE_MISS          (0x0033) enabled
#define PMCEID1_EL0_ID20 BIT(20) // Common event DTLB_WALK              (0x0034) enabled
#define PMCEID1_EL0_ID21 BIT(21) // Common event ITLB_WALK              (0x0035) enabled
#define PMCEID1_EL0_ID22 BIT(22) // Common event LL_CACHE_RD            (0x0036) enabled
#define PMCEID1_EL0_ID23 BIT(23) // Common event LL_CACHE_MISS_RD       (0x0037) enabled
#define PMCEID1_EL0_ID24 BIT(24) // Common event REMOTE_ACCESS_RD       (0x0038) enabled
#define PMCEID1_EL0_ID25 BIT(25) // Common event L1D_CACHE_LMISS_RD     (0x0039) enabled
#define PMCEID1_EL0_ID26 BIT(26) // Common event OP_RETIRED             (0x003A) enabled
#define PMCEID1_EL0_ID27 BIT(27) // Common event OP_SPEC                (0x003B) enabled
#define PMCEID1_EL0_ID28 BIT(28) // Common event STALL                  (0x003C) enabled
#define PMCEID1_EL0_ID29 BIT(29) // Common event STALL_SLOT_BACKEND     (0x003D) enabled
#define PMCEID1_EL0_ID30 BIT(30) // Common event STALL_SLOT_FRONTEND    (0x003E) enabled
#define PMCEID1_EL0_ID31 BIT(31) // Common event STALL_SLOT             (0x003F) enabled

/* Performance Monitors Cycle Count Register (PMCCNTR_EL0) bits */
#define PMCCNTR_EL0_CCNT0  BIT(00) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 0
#define PMCCNTR_EL0_CCNT1  BIT(01) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 1
#define PMCCNTR_EL0_CCNT2  BIT(02) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 2
#define PMCCNTR_EL0_CCNT3  BIT(03) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 3
#define PMCCNTR_EL0_CCNT4  BIT(04) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 4
#define PMCCNTR_EL0_CCNT5  BIT(05) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 5
#define PMCCNTR_EL0_CCNT6  BIT(06) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 6
#define PMCCNTR_EL0_CCNT7  BIT(07) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 7
#define PMCCNTR_EL0_CCNT8  BIT(08) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 8
#define PMCCNTR_EL0_CCNT9  BIT(09) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 9
#define PMCCNTR_EL0_CCNT10 BIT(10) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 10
#define PMCCNTR_EL0_CCNT11 BIT(11) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 11
#define PMCCNTR_EL0_CCNT12 BIT(12) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 12
#define PMCCNTR_EL0_CCNT13 BIT(13) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 13
#define PMCCNTR_EL0_CCNT14 BIT(14) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 14
#define PMCCNTR_EL0_CCNT15 BIT(15) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 15
#define PMCCNTR_EL0_CCNT16 BIT(16) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 16
#define PMCCNTR_EL0_CCNT17 BIT(17) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 17
#define PMCCNTR_EL0_CCNT18 BIT(18) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 18
#define PMCCNTR_EL0_CCNT19 BIT(19) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 19
#define PMCCNTR_EL0_CCNT20 BIT(20) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 20
#define PMCCNTR_EL0_CCNT21 BIT(21) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 21
#define PMCCNTR_EL0_CCNT22 BIT(22) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 22
#define PMCCNTR_EL0_CCNT23 BIT(23) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 23
#define PMCCNTR_EL0_CCNT24 BIT(24) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 24
#define PMCCNTR_EL0_CCNT25 BIT(25) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 25
#define PMCCNTR_EL0_CCNT26 BIT(26) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 26
#define PMCCNTR_EL0_CCNT27 BIT(27) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 27
#define PMCCNTR_EL0_CCNT28 BIT(28) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 28
#define PMCCNTR_EL0_CCNT29 BIT(29) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 29
#define PMCCNTR_EL0_CCNT30 BIT(30) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 30
#define PMCCNTR_EL0_CCNT31 BIT(31) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 31
#define PMCCNTR_EL0_CCNT32 BIT(32) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 32
#define PMCCNTR_EL0_CCNT33 BIT(33) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 33
#define PMCCNTR_EL0_CCNT34 BIT(34) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 34
#define PMCCNTR_EL0_CCNT35 BIT(35) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 35
#define PMCCNTR_EL0_CCNT36 BIT(36) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 36
#define PMCCNTR_EL0_CCNT37 BIT(37) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 37
#define PMCCNTR_EL0_CCNT38 BIT(38) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 38
#define PMCCNTR_EL0_CCNT39 BIT(39) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 39
#define PMCCNTR_EL0_CCNT40 BIT(40) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 40
#define PMCCNTR_EL0_CCNT41 BIT(41) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 41
#define PMCCNTR_EL0_CCNT42 BIT(42) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 42
#define PMCCNTR_EL0_CCNT43 BIT(43) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 43
#define PMCCNTR_EL0_CCNT44 BIT(44) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 44
#define PMCCNTR_EL0_CCNT45 BIT(45) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 45
#define PMCCNTR_EL0_CCNT46 BIT(46) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 46
#define PMCCNTR_EL0_CCNT47 BIT(47) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 47
#define PMCCNTR_EL0_CCNT48 BIT(48) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 48
#define PMCCNTR_EL0_CCNT49 BIT(49) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 49
#define PMCCNTR_EL0_CCNT50 BIT(50) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 50
#define PMCCNTR_EL0_CCNT51 BIT(51) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 51
#define PMCCNTR_EL0_CCNT52 BIT(52) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 52
#define PMCCNTR_EL0_CCNT53 BIT(53) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 53
#define PMCCNTR_EL0_CCNT54 BIT(54) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 54
#define PMCCNTR_EL0_CCNT55 BIT(55) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 55
#define PMCCNTR_EL0_CCNT56 BIT(56) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 56
#define PMCCNTR_EL0_CCNT57 BIT(57) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 57
#define PMCCNTR_EL0_CCNT58 BIT(58) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 58
#define PMCCNTR_EL0_CCNT59 BIT(59) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 59
#define PMCCNTR_EL0_CCNT60 BIT(60) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 60
#define PMCCNTR_EL0_CCNT61 BIT(61) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 61
#define PMCCNTR_EL0_CCNT62 BIT(62) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 62
#define PMCCNTR_EL0_CCNT63 BIT(63) // Cycle count (either every clock cycle, or every 64th) u64 0:63; bit 63

/* Performance Monitors Selected Event Type Register (PMXEVTYPER_EL0) bits */
#define PMXEVTYPER_EL0_bits0  BIT(00) // When PMSELR_EL0.SEL == 31, this register accesses PMCCFILTR_EL0. Otherwise, this register accesses PMEVTYPER<n>_EL0 where n is the value in PMSELR_EL0.SEL
#define PMXEVTYPER_EL0_bits1  BIT(01) // When PMSELR_EL0.SEL == 31, this register accesses PMCCFILTR_EL0. Otherwise, this register accesses PMEVTYPER<n>_EL0 where n is the value in PMSELR_EL0.SEL
#define PMXEVTYPER_EL0_bits2  BIT(02) // When PMSELR_EL0.SEL == 31, this register accesses PMCCFILTR_EL0. Otherwise, this register accesses PMEVTYPER<n>_EL0 where n is the value in PMSELR_EL0.SEL
#define PMXEVTYPER_EL0_bits3  BIT(03) // When PMSELR_EL0.SEL == 31, this register accesses PMCCFILTR_EL0. Otherwise, this register accesses PMEVTYPER<n>_EL0 where n is the value in PMSELR_EL0.SEL
#define PMXEVTYPER_EL0_bits4  BIT(04) // When PMSELR_EL0.SEL == 31, this register accesses PMCCFILTR_EL0. Otherwise, this register accesses PMEVTYPER<n>_EL0 where n is the value in PMSELR_EL0.SEL
#define PMXEVTYPER_EL0_bits5  BIT(05) // When PMSELR_EL0.SEL == 31, this register accesses PMCCFILTR_EL0. Otherwise, this register accesses PMEVTYPER<n>_EL0 where n is the value in PMSELR_EL0.SEL
#define PMXEVTYPER_EL0_bits6  BIT(06) // When PMSELR_EL0.SEL == 31, this register accesses PMCCFILTR_EL0. Otherwise, this register accesses PMEVTYPER<n>_EL0 where n is the value in PMSELR_EL0.SEL
#define PMXEVTYPER_EL0_bits7  BIT(07) // When PMSELR_EL0.SEL == 31, this register accesses PMCCFILTR_EL0. Otherwise, this register accesses PMEVTYPER<n>_EL0 where n is the value in PMSELR_EL0.SEL
#define PMXEVTYPER_EL0_bits8  BIT(08) // When PMSELR_EL0.SEL == 31, this register accesses PMCCFILTR_EL0. Otherwise, this register accesses PMEVTYPER<n>_EL0 where n is the value in PMSELR_EL0.SEL
#define PMXEVTYPER_EL0_bits9  BIT(09) // When PMSELR_EL0.SEL == 31, this register accesses PMCCFILTR_EL0. Otherwise, this register accesses PMEVTYPER<n>_EL0 where n is the value in PMSELR_EL0.SEL
#define PMXEVTYPER_EL0_bits10 BIT(10) // When PMSELR_EL0.SEL == 31, this register accesses PMCCFILTR_EL0. Otherwise, this register accesses PMEVTYPER<n>_EL0 where n is the value in PMSELR_EL0.SEL
#define PMXEVTYPER_EL0_bits11 BIT(11) // When PMSELR_EL0.SEL == 31, this register accesses PMCCFILTR_EL0. Otherwise, this register accesses PMEVTYPER<n>_EL0 where n is the value in PMSELR_EL0.SEL
#define PMXEVTYPER_EL0_bits12 BIT(12) // When PMSELR_EL0.SEL == 31, this register accesses PMCCFILTR_EL0. Otherwise, this register accesses PMEVTYPER<n>_EL0 where n is the value in PMSELR_EL0.SEL
#define PMXEVTYPER_EL0_bits13 BIT(13) // When PMSELR_EL0.SEL == 31, this register accesses PMCCFILTR_EL0. Otherwise, this register accesses PMEVTYPER<n>_EL0 where n is the value in PMSELR_EL0.SEL
#define PMXEVTYPER_EL0_bits14 BIT(14) // When PMSELR_EL0.SEL == 31, this register accesses PMCCFILTR_EL0. Otherwise, this register accesses PMEVTYPER<n>_EL0 where n is the value in PMSELR_EL0.SEL
#define PMXEVTYPER_EL0_bits15 BIT(15) // When PMSELR_EL0.SEL == 31, this register accesses PMCCFILTR_EL0. Otherwise, this register accesses PMEVTYPER<n>_EL0 where n is the value in PMSELR_EL0.SEL
#define PMXEVTYPER_EL0_bits16 BIT(16) // When PMSELR_EL0.SEL == 31, this register accesses PMCCFILTR_EL0. Otherwise, this register accesses PMEVTYPER<n>_EL0 where n is the value in PMSELR_EL0.SEL
#define PMXEVTYPER_EL0_bits17 BIT(17) // When PMSELR_EL0.SEL == 31, this register accesses PMCCFILTR_EL0. Otherwise, this register accesses PMEVTYPER<n>_EL0 where n is the value in PMSELR_EL0.SEL
#define PMXEVTYPER_EL0_bits18 BIT(18) // When PMSELR_EL0.SEL == 31, this register accesses PMCCFILTR_EL0. Otherwise, this register accesses PMEVTYPER<n>_EL0 where n is the value in PMSELR_EL0.SEL
#define PMXEVTYPER_EL0_bits19 BIT(19) // When PMSELR_EL0.SEL == 31, this register accesses PMCCFILTR_EL0. Otherwise, this register accesses PMEVTYPER<n>_EL0 where n is the value in PMSELR_EL0.SEL
#define PMXEVTYPER_EL0_bits20 BIT(20) // When PMSELR_EL0.SEL == 31, this register accesses PMCCFILTR_EL0. Otherwise, this register accesses PMEVTYPER<n>_EL0 where n is the value in PMSELR_EL0.SEL
#define PMXEVTYPER_EL0_bits21 BIT(21) // When PMSELR_EL0.SEL == 31, this register accesses PMCCFILTR_EL0. Otherwise, this register accesses PMEVTYPER<n>_EL0 where n is the value in PMSELR_EL0.SEL
#define PMXEVTYPER_EL0_bits22 BIT(22) // When PMSELR_EL0.SEL == 31, this register accesses PMCCFILTR_EL0. Otherwise, this register accesses PMEVTYPER<n>_EL0 where n is the value in PMSELR_EL0.SEL
#define PMXEVTYPER_EL0_bits23 BIT(23) // When PMSELR_EL0.SEL == 31, this register accesses PMCCFILTR_EL0. Otherwise, this register accesses PMEVTYPER<n>_EL0 where n is the value in PMSELR_EL0.SEL
#define PMXEVTYPER_EL0_bits24 BIT(24) // When PMSELR_EL0.SEL == 31, this register accesses PMCCFILTR_EL0. Otherwise, this register accesses PMEVTYPER<n>_EL0 where n is the value in PMSELR_EL0.SEL
#define PMXEVTYPER_EL0_bits25 BIT(25) // When PMSELR_EL0.SEL == 31, this register accesses PMCCFILTR_EL0. Otherwise, this register accesses PMEVTYPER<n>_EL0 where n is the value in PMSELR_EL0.SEL
#define PMXEVTYPER_EL0_bits26 BIT(26) // When PMSELR_EL0.SEL == 31, this register accesses PMCCFILTR_EL0. Otherwise, this register accesses PMEVTYPER<n>_EL0 where n is the value in PMSELR_EL0.SEL
#define PMXEVTYPER_EL0_bits27 BIT(27) // When PMSELR_EL0.SEL == 31, this register accesses PMCCFILTR_EL0. Otherwise, this register accesses PMEVTYPER<n>_EL0 where n is the value in PMSELR_EL0.SEL
#define PMXEVTYPER_EL0_bits28 BIT(28) // When PMSELR_EL0.SEL == 31, this register accesses PMCCFILTR_EL0. Otherwise, this register accesses PMEVTYPER<n>_EL0 where n is the value in PMSELR_EL0.SEL
#define PMXEVTYPER_EL0_bits29 BIT(29) // When PMSELR_EL0.SEL == 31, this register accesses PMCCFILTR_EL0. Otherwise, this register accesses PMEVTYPER<n>_EL0 where n is the value in PMSELR_EL0.SEL
#define PMXEVTYPER_EL0_bits30 BIT(30) // When PMSELR_EL0.SEL == 31, this register accesses PMCCFILTR_EL0. Otherwise, this register accesses PMEVTYPER<n>_EL0 where n is the value in PMSELR_EL0.SEL
#define PMXEVTYPER_EL0_bits31 BIT(31) // When PMSELR_EL0.SEL == 31, this register accesses PMCCFILTR_EL0. Otherwise, this register accesses PMEVTYPER<n>_EL0 where n is the value in PMSELR_EL0.SEL

/* Performance Monitors Cycle Count Filter Register (PMCCFILTR_EL0) bits */
#define PMCCFILTR_EL0_RES0  BIT(00) // Reserved
#define PMCCFILTR_EL0_RES1  BIT(01) // Reserved
#define PMCCFILTR_EL0_RES2  BIT(02) // Reserved
#define PMCCFILTR_EL0_RES3  BIT(03) // Reserved
#define PMCCFILTR_EL0_RES4  BIT(04) // Reserved
#define PMCCFILTR_EL0_RES5  BIT(05) // Reserved
#define PMCCFILTR_EL0_RES6  BIT(06) // Reserved
#define PMCCFILTR_EL0_RES7  BIT(07) // Reserved
#define PMCCFILTR_EL0_RES8  BIT(08) // Reserved
#define PMCCFILTR_EL0_RES9  BIT(09) // Reserved
#define PMCCFILTR_EL0_RES10 BIT(10) // Reserved
#define PMCCFILTR_EL0_RES11 BIT(11) // Reserved
#define PMCCFILTR_EL0_RES12 BIT(12) // Reserved
#define PMCCFILTR_EL0_RES13 BIT(13) // Reserved
#define PMCCFILTR_EL0_RES14 BIT(14) // Reserved
#define PMCCFILTR_EL0_RES15 BIT(15) // Reserved
#define PMCCFILTR_EL0_RES16 BIT(16) // Reserved
#define PMCCFILTR_EL0_RES17 BIT(17) // Reserved
#define PMCCFILTR_EL0_RES18 BIT(18) // Reserved
#define PMCCFILTR_EL0_RES19 BIT(19) // Reserved
#define PMCCFILTR_EL0_RES20 BIT(20) // Reserved
#define PMCCFILTR_EL0_RES21 BIT(21) // Reserved
#define PMCCFILTR_EL0_RES22 BIT(22) // Reserved
#define PMCCFILTR_EL0_RES23 BIT(23) // Reserved
#define PMCCFILTR_EL0_RES24 BIT(24) // Reserved
#define PMCCFILTR_EL0_RES25 BIT(25) // Reserved
#define PMCCFILTR_EL0_RES26 BIT(26) // Reserved
#define PMCCFILTR_EL0_RES27 BIT(27) // Reserved
#define PMCCFILTR_EL0_RES28 BIT(28) // Reserved
#define PMCCFILTR_EL0_RES29 BIT(29) // Reserved
#define PMCCFILTR_EL0_U     BIT(30) // User filtering bit (EL0)
#define PMCCFILTR_EL0_P     BIT(31) // Privileged filtering bit (EL1)

/* Performance Monitors Selected Event Count Register (PMXEVCNTR_EL0) bits */
#define PMXEVCNTR_EL0_BITS0  BIT(00) // Value of the selected event counter, PMEVCNTR<n>_EL0, where n is the value stored in PMSELR_EL0.SEL
#define PMXEVCNTR_EL0_BITS1  BIT(01) // Value of the selected event counter, PMEVCNTR<n>_EL0, where n is the value stored in PMSELR_EL0.SEL
#define PMXEVCNTR_EL0_BITS2  BIT(02) // Value of the selected event counter, PMEVCNTR<n>_EL0, where n is the value stored in PMSELR_EL0.SEL
#define PMXEVCNTR_EL0_BITS3  BIT(03) // Value of the selected event counter, PMEVCNTR<n>_EL0, where n is the value stored in PMSELR_EL0.SEL
#define PMXEVCNTR_EL0_BITS4  BIT(04) // Value of the selected event counter, PMEVCNTR<n>_EL0, where n is the value stored in PMSELR_EL0.SEL
#define PMXEVCNTR_EL0_BITS5  BIT(05) // Value of the selected event counter, PMEVCNTR<n>_EL0, where n is the value stored in PMSELR_EL0.SEL
#define PMXEVCNTR_EL0_BITS6  BIT(06) // Value of the selected event counter, PMEVCNTR<n>_EL0, where n is the value stored in PMSELR_EL0.SEL
#define PMXEVCNTR_EL0_BITS7  BIT(07) // Value of the selected event counter, PMEVCNTR<n>_EL0, where n is the value stored in PMSELR_EL0.SEL
#define PMXEVCNTR_EL0_BITS8  BIT(08) // Value of the selected event counter, PMEVCNTR<n>_EL0, where n is the value stored in PMSELR_EL0.SEL
#define PMXEVCNTR_EL0_BITS9  BIT(09) // Value of the selected event counter, PMEVCNTR<n>_EL0, where n is the value stored in PMSELR_EL0.SEL
#define PMXEVCNTR_EL0_BITS10 BIT(10) // Value of the selected event counter, PMEVCNTR<n>_EL0, where n is the value stored in PMSELR_EL0.SEL
#define PMXEVCNTR_EL0_BITS11 BIT(11) // Value of the selected event counter, PMEVCNTR<n>_EL0, where n is the value stored in PMSELR_EL0.SEL
#define PMXEVCNTR_EL0_BITS12 BIT(12) // Value of the selected event counter, PMEVCNTR<n>_EL0, where n is the value stored in PMSELR_EL0.SEL
#define PMXEVCNTR_EL0_BITS13 BIT(13) // Value of the selected event counter, PMEVCNTR<n>_EL0, where n is the value stored in PMSELR_EL0.SEL
#define PMXEVCNTR_EL0_BITS14 BIT(14) // Value of the selected event counter, PMEVCNTR<n>_EL0, where n is the value stored in PMSELR_EL0.SEL
#define PMXEVCNTR_EL0_BITS15 BIT(15) // Value of the selected event counter, PMEVCNTR<n>_EL0, where n is the value stored in PMSELR_EL0.SEL
#define PMXEVCNTR_EL0_BITS16 BIT(16) // Value of the selected event counter, PMEVCNTR<n>_EL0, where n is the value stored in PMSELR_EL0.SEL
#define PMXEVCNTR_EL0_BITS17 BIT(17) // Value of the selected event counter, PMEVCNTR<n>_EL0, where n is the value stored in PMSELR_EL0.SEL
#define PMXEVCNTR_EL0_BITS18 BIT(18) // Value of the selected event counter, PMEVCNTR<n>_EL0, where n is the value stored in PMSELR_EL0.SEL
#define PMXEVCNTR_EL0_BITS19 BIT(19) // Value of the selected event counter, PMEVCNTR<n>_EL0, where n is the value stored in PMSELR_EL0.SEL
#define PMXEVCNTR_EL0_BITS20 BIT(20) // Value of the selected event counter, PMEVCNTR<n>_EL0, where n is the value stored in PMSELR_EL0.SEL
#define PMXEVCNTR_EL0_BITS21 BIT(21) // Value of the selected event counter, PMEVCNTR<n>_EL0, where n is the value stored in PMSELR_EL0.SEL
#define PMXEVCNTR_EL0_BITS22 BIT(22) // Value of the selected event counter, PMEVCNTR<n>_EL0, where n is the value stored in PMSELR_EL0.SEL
#define PMXEVCNTR_EL0_BITS23 BIT(23) // Value of the selected event counter, PMEVCNTR<n>_EL0, where n is the value stored in PMSELR_EL0.SEL
#define PMXEVCNTR_EL0_BITS24 BIT(24) // Value of the selected event counter, PMEVCNTR<n>_EL0, where n is the value stored in PMSELR_EL0.SEL
#define PMXEVCNTR_EL0_BITS25 BIT(25) // Value of the selected event counter, PMEVCNTR<n>_EL0, where n is the value stored in PMSELR_EL0.SEL
#define PMXEVCNTR_EL0_BITS26 BIT(26) // Value of the selected event counter, PMEVCNTR<n>_EL0, where n is the value stored in PMSELR_EL0.SEL
#define PMXEVCNTR_EL0_BITS27 BIT(27) // Value of the selected event counter, PMEVCNTR<n>_EL0, where n is the value stored in PMSELR_EL0.SEL
#define PMXEVCNTR_EL0_BITS28 BIT(28) // Value of the selected event counter, PMEVCNTR<n>_EL0, where n is the value stored in PMSELR_EL0.SEL
#define PMXEVCNTR_EL0_BITS29 BIT(29) // Value of the selected event counter, PMEVCNTR<n>_EL0, where n is the value stored in PMSELR_EL0.SEL
#define PMXEVCNTR_EL0_BITS30 BIT(30) // Value of the selected event counter, PMEVCNTR<n>_EL0, where n is the value stored in PMSELR_EL0.SEL
#define PMXEVCNTR_EL0_BITS31 BIT(31) // Value of the selected event counter, PMEVCNTR<n>_EL0, where n is the value stored in PMSELR_EL0.SEL

/* Performance Monitors User Enable Register (PMUSERENR_EL0) bits */
#define PMUSERENR_EL0_EN    BIT(00) // Traps EL0 accesses to the Performance Monitor registers to EL1, or to EL2
#define PMUSERENR_EL0_SW    BIT(01) // Traps Software Increment writes to EL1, or to EL2
#define PMUSERENR_EL0_CR    BIT(02) // Cycle counter Read. Traps EL0 access to cycle counter reads to EL1, or to EL2
#define PMUSERENR_EL0_ER    BIT(03) // Event counter Read. Traps EL0 access to event counters to EL1, or to EL2
#define PMUSERENR_EL0_RES0  BIT(04) // Reserved
#define PMUSERENR_EL0_RES1  BIT(05) // Reserved
#define PMUSERENR_EL0_RES2  BIT(06) // Reserved
#define PMUSERENR_EL0_RES3  BIT(07) // Reserved
#define PMUSERENR_EL0_RES4  BIT(08) // Reserved
#define PMUSERENR_EL0_RES5  BIT(09) // Reserved
#define PMUSERENR_EL0_RES6  BIT(10) // Reserved
#define PMUSERENR_EL0_RES7  BIT(11) // Reserved
#define PMUSERENR_EL0_RES8  BIT(12) // Reserved
#define PMUSERENR_EL0_RES9  BIT(13) // Reserved
#define PMUSERENR_EL0_RES10 BIT(14) // Reserved
#define PMUSERENR_EL0_RES11 BIT(15) // Reserved
#define PMUSERENR_EL0_RES12 BIT(16) // Reserved
#define PMUSERENR_EL0_RES13 BIT(17) // Reserved
#define PMUSERENR_EL0_RES14 BIT(18) // Reserved
#define PMUSERENR_EL0_RES15 BIT(19) // Reserved
#define PMUSERENR_EL0_RES16 BIT(20) // Reserved
#define PMUSERENR_EL0_RES17 BIT(21) // Reserved
#define PMUSERENR_EL0_RES18 BIT(22) // Reserved
#define PMUSERENR_EL0_RES19 BIT(23) // Reserved
#define PMUSERENR_EL0_RES20 BIT(24) // Reserved
#define PMUSERENR_EL0_RES21 BIT(25) // Reserved
#define PMUSERENR_EL0_RES22 BIT(26) // Reserved
#define PMUSERENR_EL0_RES23 BIT(27) // Reserved
#define PMUSERENR_EL0_RES24 BIT(28) // Reserved
#define PMUSERENR_EL0_RES25 BIT(29) // Reserved
#define PMUSERENR_EL0_RES26 BIT(30) // Reserved
#define PMUSERENR_EL0_RES27 BIT(31) // Reserved

/* Performance Monitors Interrupt Enable Set register (PMINTENSET_EL1) bits */
#define PMINTENSET_EL1_P0    BIT(00) // Event counter overflow interrupt request enable bit for PMEVCNTR0_EL0
#define PMINTENSET_EL1_P1    BIT(01) // Event counter overflow interrupt request enable bit for PMEVCNTR1_EL0
#define PMINTENSET_EL1_P2    BIT(02) // Event counter overflow interrupt request enable bit for PMEVCNTR2_EL0
#define PMINTENSET_EL1_P3    BIT(03) // Event counter overflow interrupt request enable bit for PMEVCNTR3_EL0
#define PMINTENSET_EL1_P4    BIT(04) // Event counter overflow interrupt request enable bit for PMEVCNTR4_EL0
#define PMINTENSET_EL1_RES0  BIT(05) // Reserved
#define PMINTENSET_EL1_RES1  BIT(06) // Reserved
#define PMINTENSET_EL1_RES2  BIT(07) // Reserved
#define PMINTENSET_EL1_RES3  BIT(08) // Reserved
#define PMINTENSET_EL1_RES4  BIT(09) // Reserved
#define PMINTENSET_EL1_RES5  BIT(10) // Reserved
#define PMINTENSET_EL1_RES6  BIT(11) // Reserved
#define PMINTENSET_EL1_RES7  BIT(12) // Reserved
#define PMINTENSET_EL1_RES8  BIT(13) // Reserved
#define PMINTENSET_EL1_RES9  BIT(14) // Reserved
#define PMINTENSET_EL1_RES10 BIT(15) // Reserved
#define PMINTENSET_EL1_RES11 BIT(16) // Reserved
#define PMINTENSET_EL1_RES12 BIT(17) // Reserved
#define PMINTENSET_EL1_RES13 BIT(18) // Reserved
#define PMINTENSET_EL1_RES14 BIT(19) // Reserved
#define PMINTENSET_EL1_RES15 BIT(20) // Reserved
#define PMINTENSET_EL1_RES16 BIT(21) // Reserved
#define PMINTENSET_EL1_RES17 BIT(22) // Reserved
#define PMINTENSET_EL1_RES18 BIT(23) // Reserved
#define PMINTENSET_EL1_RES19 BIT(24) // Reserved
#define PMINTENSET_EL1_RES20 BIT(25) // Reserved
#define PMINTENSET_EL1_RES21 BIT(26) // Reserved
#define PMINTENSET_EL1_RES22 BIT(27) // Reserved
#define PMINTENSET_EL1_RES23 BIT(28) // Reserved
#define PMINTENSET_EL1_RES24 BIT(29) // Reserved
#define PMINTENSET_EL1_RES25 BIT(30) // Reserved
#define PMINTENSET_EL1_C     BIT(31) // PMCCNTR_EL0 overflow interrupt request enable bit.

/* Performance Monitors Interrupt Enable Clear register (PMINTENCLR_EL1) bits */
#define PMINTENCLR_EL1_P0    BIT(00) // Event counter overflow interrupt request dsiable bit for PMEVCNTR0_EL0
#define PMINTENCLR_EL1_P1    BIT(01) // Event counter overflow interrupt request dsiable bit for PMEVCNTR1_EL0
#define PMINTENCLR_EL1_P2    BIT(02) // Event counter overflow interrupt request dsiable bit for PMEVCNTR2_EL0
#define PMINTENCLR_EL1_P3    BIT(03) // Event counter overflow interrupt request dsiable bit for PMEVCNTR3_EL0
#define PMINTENCLR_EL1_P4    BIT(04) // Event counter overflow interrupt request dsiable bit for PMEVCNTR4_EL0
#define PMINTENCLR_EL1_RES0  BIT(05) // Reserved
#define PMINTENCLR_EL1_RES1  BIT(06) // Reserved
#define PMINTENCLR_EL1_RES2  BIT(07) // Reserved
#define PMINTENCLR_EL1_RES3  BIT(08) // Reserved
#define PMINTENCLR_EL1_RES4  BIT(09) // Reserved
#define PMINTENCLR_EL1_RES5  BIT(10) // Reserved
#define PMINTENCLR_EL1_RES6  BIT(11) // Reserved
#define PMINTENCLR_EL1_RES7  BIT(12) // Reserved
#define PMINTENCLR_EL1_RES8  BIT(13) // Reserved
#define PMINTENCLR_EL1_RES9  BIT(14) // Reserved
#define PMINTENCLR_EL1_RES10 BIT(15) // Reserved
#define PMINTENCLR_EL1_RES11 BIT(16) // Reserved
#define PMINTENCLR_EL1_RES12 BIT(17) // Reserved
#define PMINTENCLR_EL1_RES13 BIT(18) // Reserved
#define PMINTENCLR_EL1_RES14 BIT(19) // Reserved
#define PMINTENCLR_EL1_RES15 BIT(20) // Reserved
#define PMINTENCLR_EL1_RES16 BIT(21) // Reserved
#define PMINTENCLR_EL1_RES17 BIT(22) // Reserved
#define PMINTENCLR_EL1_RES18 BIT(23) // Reserved
#define PMINTENCLR_EL1_RES19 BIT(24) // Reserved
#define PMINTENCLR_EL1_RES20 BIT(25) // Reserved
#define PMINTENCLR_EL1_RES21 BIT(26) // Reserved
#define PMINTENCLR_EL1_RES22 BIT(27) // Reserved
#define PMINTENCLR_EL1_RES23 BIT(28) // Reserved
#define PMINTENCLR_EL1_RES24 BIT(29) // Reserved
#define PMINTENCLR_EL1_RES25 BIT(30) // Reserved
#define PMINTENCLR_EL1_C     BIT(31) // PMCCNTR_EL0 overflow interrupt request disable bit.

/* Performance Monitors Overflow Flag Status Set register (PMOVSSET_EL0) bits */
#define PMOVSSET_EL0_P0    BIT(00) // Event counter overflow set bit for PMEVCNTR0_EL0
#define PMOVSSET_EL0_P1    BIT(01) // Event counter overflow set bit for PMEVCNTR1_EL0
#define PMOVSSET_EL0_P2    BIT(02) // Event counter overflow set bit for PMEVCNTR2_EL0
#define PMOVSSET_EL0_P3    BIT(03) // Event counter overflow set bit for PMEVCNTR3_EL0
#define PMOVSSET_EL0_P4    BIT(04) // Event counter overflow set bit for PMEVCNTR4_EL0
#define PMOVSSET_EL0_RES0  BIT(05) // Event counter overflow set bit for PMEVCNTR5_EL0
#define PMOVSSET_EL0_RES1  BIT(06) // Reserved
#define PMOVSSET_EL0_RES2  BIT(07) // Reserved
#define PMOVSSET_EL0_RES3  BIT(08) // Reserved
#define PMOVSSET_EL0_RES4  BIT(09) // Reserved
#define PMOVSSET_EL0_RES5  BIT(10) // Reserved
#define PMOVSSET_EL0_RES6  BIT(11) // Reserved
#define PMOVSSET_EL0_RES7  BIT(12) // Reserved
#define PMOVSSET_EL0_RES8  BIT(13) // Reserved
#define PMOVSSET_EL0_RES9  BIT(14) // Reserved
#define PMOVSSET_EL0_RES10 BIT(15) // Reserved
#define PMOVSSET_EL0_RES11 BIT(16) // Reserved
#define PMOVSSET_EL0_RES12 BIT(17) // Reserved
#define PMOVSSET_EL0_RES13 BIT(18) // Reserved
#define PMOVSSET_EL0_RES14 BIT(19) // Reserved
#define PMOVSSET_EL0_RES15 BIT(20) // Reserved
#define PMOVSSET_EL0_RES16 BIT(21) // Reserved
#define PMOVSSET_EL0_RES17 BIT(22) // Reserved
#define PMOVSSET_EL0_RES18 BIT(23) // Reserved
#define PMOVSSET_EL0_RES19 BIT(24) // Reserved
#define PMOVSSET_EL0_RES20 BIT(25) // Reserved
#define PMOVSSET_EL0_RES21 BIT(26) // Reserved
#define PMOVSSET_EL0_RES22 BIT(27) // Reserved
#define PMOVSSET_EL0_RES23 BIT(28) // Reserved
#define PMOVSSET_EL0_RES24 BIT(29) // Reserved
#define PMOVSSET_EL0_RES25 BIT(30) // Reserved
#define PMOVSSET_EL0_C     BIT(31) // Cycle counter overflow set bit

/* PMEVCNTR_EL0 bits */
#define PMEVCNTR_EL0_BITS0  BIT(00) // Event counter 0. Value of event counter 0, bit 0
#define PMEVCNTR_EL0_BITS1  BIT(01) // Event counter 1. Value of event counter 1, bit 1
#define PMEVCNTR_EL0_BITS2  BIT(02) // Event counter 2. Value of event counter 2, bit 2
#define PMEVCNTR_EL0_BITS3  BIT(03) // Event counter 3. Value of event counter 3, bit 3
#define PMEVCNTR_EL0_BITS4  BIT(04) // Event counter 4. Value of event counter 4, bit 4
#define PMEVCNTR_EL0_BITS5  BIT(05) // Event counter 5. Value of event counter 5, bit 5
#define PMEVCNTR_EL0_BITS6  BIT(06) // Event counter 6. Value of event counter 6, bit 6
#define PMEVCNTR_EL0_BITS7  BIT(07) // Event counter 7. Value of event counter 7, bit 7
#define PMEVCNTR_EL0_BITS8  BIT(08) // Event counter 8. Value of event counter 8, bit 8
#define PMEVCNTR_EL0_BITS9  BIT(09) // Event counter 9. Value of event counter 9, bit 9
#define PMEVCNTR_EL0_BITS10 BIT(10) // Event counter 10. Value of event counter 10, bit 10
#define PMEVCNTR_EL0_BITS11 BIT(11) // Event counter 11. Value of event counter 11, bit 11
#define PMEVCNTR_EL0_BITS12 BIT(12) // Event counter 12. Value of event counter 12, bit 12
#define PMEVCNTR_EL0_BITS13 BIT(13) // Event counter 13. Value of event counter 13, bit 13
#define PMEVCNTR_EL0_BITS14 BIT(14) // Event counter 14. Value of event counter 14, bit 14
#define PMEVCNTR_EL0_BITS15 BIT(15) // Event counter 15. Value of event counter 15, bit 15
#define PMEVCNTR_EL0_BITS16 BIT(16) // Event counter 16. Value of event counter 16, bit 16
#define PMEVCNTR_EL0_BITS17 BIT(17) // Event counter 17. Value of event counter 17, bit 17
#define PMEVCNTR_EL0_BITS18 BIT(18) // Event counter 18. Value of event counter 18, bit 18
#define PMEVCNTR_EL0_BITS19 BIT(19) // Event counter 19. Value of event counter 19, bit 19
#define PMEVCNTR_EL0_BITS20 BIT(20) // Event counter 20. Value of event counter 20, bit 20
#define PMEVCNTR_EL0_BITS21 BIT(21) // Event counter 21. Value of event counter 21, bit 21
#define PMEVCNTR_EL0_BITS22 BIT(22) // Event counter 22. Value of event counter 22, bit 22
#define PMEVCNTR_EL0_BITS23 BIT(23) // Event counter 23. Value of event counter 23, bit 23
#define PMEVCNTR_EL0_BITS24 BIT(24) // Event counter 24. Value of event counter 24, bit 24
#define PMEVCNTR_EL0_BITS25 BIT(25) // Event counter 25. Value of event counter 25, bit 25
#define PMEVCNTR_EL0_BITS26 BIT(26) // Event counter 26. Value of event counter 26, bit 26
#define PMEVCNTR_EL0_BITS27 BIT(27) // Event counter 27. Value of event counter 27, bit 27
#define PMEVCNTR_EL0_BITS28 BIT(28) // Event counter 28. Value of event counter 28, bit 28
#define PMEVCNTR_EL0_BITS29 BIT(29) // Event counter 29. Value of event counter 29, bit 29
#define PMEVCNTR_EL0_BITS30 BIT(30) // Event counter 30. Value of event counter 30, bit 30
#define PMEVCNTR_EL0_BITS31 BIT(31) // Event counter 31. Value of event counter 31, bit 31

/* Performance Monitors Event Type Registers (PMEVTYPER_EL0) bits */
#define PMEVTYPER_EL0_EVTCOUNT0 BIT(00) // Event to count. The event number of the event that is counted by event counter
#define PMEVTYPER_EL0_EVTCOUNT1 BIT(01) // Event to count. The event number of the event that is counted by event counter
#define PMEVTYPER_EL0_EVTCOUNT2 BIT(02) // Event to count. The event number of the event that is counted by event counter
#define PMEVTYPER_EL0_EVTCOUNT3 BIT(03) // Event to count. The event number of the event that is counted by event counter
#define PMEVTYPER_EL0_EVTCOUNT4 BIT(04) // Event to count. The event number of the event that is counted by event counter
#define PMEVTYPER_EL0_EVTCOUNT5 BIT(05) // Event to count. The event number of the event that is counted by event counter
#define PMEVTYPER_EL0_EVTCOUNT6 BIT(06) // Event to count. The event number of the event that is counted by event counter
#define PMEVTYPER_EL0_EVTCOUNT7 BIT(07) // Event to count. The event number of the event that is counted by event counter
#define PMEVTYPER_EL0_EVTCOUNT8 BIT(08) // Event to count. The event number of the event that is counted by event counter
#define PMEVTYPER_EL0_EVTCOUNT9 BIT(09) // Event to count. The event number of the event that is counted by event counter
#define PMEVTYPER_EL0_RES0      BIT(10) // Reserved
#define PMEVTYPER_EL0_RES1      BIT(11) // Reserved
#define PMEVTYPER_EL0_RES2      BIT(12) // Reserved
#define PMEVTYPER_EL0_RES3      BIT(13) // Reserved
#define PMEVTYPER_EL0_RES4      BIT(14) // Reserved
#define PMEVTYPER_EL0_RES5      BIT(15) // Reserved
#define PMEVTYPER_EL0_RES6      BIT(16) // Reserved
#define PMEVTYPER_EL0_RES7      BIT(17) // Reserved
#define PMEVTYPER_EL0_RES8      BIT(18) // Reserved
#define PMEVTYPER_EL0_RES9      BIT(19) // Reserved
#define PMEVTYPER_EL0_RES10     BIT(20) // Reserved
#define PMEVTYPER_EL0_RES11     BIT(21) // Reserved
#define PMEVTYPER_EL0_RES12     BIT(22) // Reserved
#define PMEVTYPER_EL0_RES13     BIT(23) // Reserved
#define PMEVTYPER_EL0_SH        BIT(24) // Secure EL2 filtering
#define PMEVTYPER_EL0_RES15     BIT(25) // Reserved
#define PMEVTYPER_EL0_M         BIT(26) // Secure EL3 filtering
#define PMEVTYPER_EL0_RES17     BIT(27) // Reserved
#define PMEVTYPER_EL0_NSU       BIT(28) // Non-secure EL0 (Unprivileged) filtering bit.
#define PMEVTYPER_EL0_NSK       BIT(29) // Non-secure EL1 (Kernel) filtering bit.
#define PMEVTYPER_EL0_U         BIT(30) // User filtering bit. Controls counting in EL0
#define PMEVTYPER_EL0_P         BIT(31) // Privileged filtering bit. Controls counting in EL1

/* Event counter register values */
#define PMEVCNTR0_EL0 0 // Event counter 0
#define PMEVCNTR1_EL0 1 // Event counter 1
#define PMEVCNTR2_EL0 2 // Event counter 2
#define PMEVCNTR3_EL0 3 // Event counter 3
#define PMEVCNTR4_EL0 4 // Event counter 4
#define PMEVCNTR5_EL0 5 // Event counter 5

/* Event counter configurer register values */
#define PMEVTYPER0_EL0 0 // Event counter conifigurer 0
#define PMEVTYPER1_EL0 1 // Event counter conifigurer 1
#define PMEVTYPER2_EL0 2 // Event counter conifigurer 2
#define PMEVTYPER3_EL0 3 // Event counter conifigurer 3
#define PMEVTYPER4_EL0 4 // Event counter conifigurer 4
#define PMEVTYPER5_EL0 5 // Event counter conifigurer 5

// Don't ask...
#define tostr(S) #S

/* Getters */
#define GET_PMEVTYPERN_EL0(N, OUT) asm volatile("mrs %0, pmevtyper" tostr(N) "_el0" : "=r"(OUT) : : "memory")
#define GET_PMEVCNTRN_EL0(N, OUT)  asm volatile("mrs %0, pmevcntr" tostr(N) "_el0"  : "=r"(OUT) : : "memory")
// PMOVSSET_EL0 is not readable in EL0
// PMINTENCLR_EL1 is not readable in EL0
#define GET_PMINTENSET_EL1(OUT)    asm volatile("mrs %0, pmintenset_el1"            : "=r"(OUT) : : "memory")
#define GET_PMUSERENR_EL0(OUT)     asm volatile("mrs %0, pmuserenr_el0"             : "=r"(OUT) : : "memory")
#define GET_PMXEVCNTR_EL0(OUT)     asm volatile("mrs %0, pmxevcntr_el0"             : "=r"(OUT) : : "memory")
#define GET_PMCCFILTR_EL0(OUT)     asm volatile("mrs %0, pmccfiltr_el0"             : "=r"(OUT) : : "memory")
#define GET_PMXEVTYPER_EL0(OUT)    asm volatile("mrs %0, pmxevtyper_el0"            : "=r"(OUT) : : "memory")
#define GET_PMCCNTR_EL0(OUT)       asm volatile("mrs %0, pmccntr_el0"               : "=r"(OUT) : : "memory")
#define GET_PMCEIDN_EL0(N, OUT)    asm volatile("mrs %0, pmceid" tostr(N) "_el0"    : "=r"(OUT) : : "memory")
#define GET_PMSELR_EL0(OUT)        asm volatile("mrs %0, pmselr_el0"                : "=r"(OUT) : : "memory")
// PMSWINC_EL0 is write only
#define GET_PMOVSCLR_EL0(OUT)      asm volatile("mrs %0, pmovsclr_el0"              : "=r"(OUT) : : "memory")
#define GET_PMCNTENCLR_EL0(OUT)    asm volatile("mrs %0, pmcntenclr_el0"            : "=r"(OUT) : : "memory")
#define GET_PMCNTENSET_EL0(OUT)    asm volatile("mrs %0, pmcntenset_el0"            : "=r"(OUT) : : "memory")
#define GET_PMCR_EL0(OUT)          asm volatile("mrs %0, pmcr_el0"                  : "=r"(OUT) : : "memory")

/* Setters */
#define SET_PMEVTYPERN_EL0(N, IN)  asm volatile("msr pmevtyper" tostr(N) "_el0, %0" : : "r"(IN) :   "memory", "cc")
#define SET_PMEVCNTRN_EL0(N, IN)   asm volatile("msr pmevcntr" tostr(N) "_el0, %0"  : : "r"(IN) :   "memory", "cc")
#define SET_PMOVSSET_EL0(IN)       asm volatile("msr pmovsset_el0, %0"              : : "r"(IN) :   "memory", "cc")
// PMINTENCLR_EL1 is not writable in EL0
// PMINTENSET_EL1 is not writable in EL0
// PMUSERENR_EL0 is not writable in EL0
#define SET_PMXEVCNTR_EL0(IN)      asm volatile("msr pmxevcntr_el0, %0"             : : "r"(IN) :   "memory", "cc")
#define SET_PMCCFILTR_EL0(IN)      asm volatile("msr pmccfiltr_el0, %0"             : : "r"(IN) :   "memory", "cc")
#define SET_PMXEVTYPER_EL0(IN)     asm volatile("msr pmxevtyper_el0, %0"            : : "r"(IN) :   "memory", "cc")
#define SET_PMCCNTR_EL0(IN)        asm volatile("msr pmccntr_el0, %0"               : : "r"(IN) :   "memory", "cc")
// PMCEID0/1_ELO is read only
#define SET_PMSELR_EL0(IN)         asm volatile("msr pmselr_el0, %0"                : : "r"(IN) :   "memory", "cc")
#define SET_PMSWINC_EL0(IN)        asm volatile("msr pmswinc_el0, %0"               : : "r"(IN) :   "memory", "cc")
#define SET_PMOVSCLR_EL0(IN)       asm volatile("msr pmovsclr_el0, %0"              : : "r"(IN) :   "memory", "cc")
#define SET_PMCNTENCLR_EL0(IN)     asm volatile("msr pmcntenclr_el0, %0"            : : "r"(IN) :   "memory", "cc")
#define SET_PMCNTENSET_EL0(IN)     asm volatile("msr pmcntenset_el0, %0"            : : "r"(IN) :   "memory", "cc")
#define SET_PMCR_EL0(IN)           asm volatile("msr pmcr_el0, %0"                  : : "r"(IN) :   "memory", "cc")

/* Utilities */
#define CLOBBER_MEMORY()          asm volatile(""       : :          : "memory")
#define DO_NOT_OPTIMIZE(F)        asm volatile(""       : : "r,m"(F) : "memory")

/* Barriers */
#define INSTRUCTION_BARRIER()     asm volatile("isb sy" : :          : "memory")
#define DATA_SYNC_BARRIER()       asm volatile("dsb sy" : :          : "memory")

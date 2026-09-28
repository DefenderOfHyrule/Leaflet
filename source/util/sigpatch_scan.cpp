#include <switch.h>
#include <cstring>
#include <span>
#include <vector>
#include <iterator>
#include "util/sigpatch_scan.hpp"

namespace inst::util {
    namespace {
        constexpr u32 FW_VER_ANY = 0x0;
        constexpr u16 REGEX_SKIP = 0x100;

        // fs .text is well under this; the cap just stops a bogus region eating the heap.
        constexpr u64 MAX_REGION_SIZE = 0x800000;

        constexpr u64 FS_PROGRAM_ID  = 0x0100000000000000;
        constexpr u64 LDR_PROGRAM_ID = 0x0100000000000001;

        template<typename T>
        constexpr void str2hex(const char* s, T* data, u8& size) {
            if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
                s += 2;
            }

            constexpr auto hexstr_2_nibble = [](char c) -> u8 {
                if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
                if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
                if (c >= '0' && c <= '9') { return c - '0'; }
                return 0;
            };

            while (*s != '\0') {
                if (sizeof(T) == sizeof(u16) && *s == '.') {
                    data[size] = REGEX_SKIP;
                    s += 2; // consume both dots of ".."
                } else {
                    data[size] |= hexstr_2_nibble(*s++) << 4;
                    data[size] |= hexstr_2_nibble(*s++) << 0;
                }
                size++;
            }
        }

        struct PatternData {
            constexpr PatternData(const char* s) {
                str2hex(s, data, size);
            }

            u16 data[60]{};
            u8 size{};
        };

        struct PatchData {
            constexpr PatchData(const char* s) {
                str2hex(s, data, size);
            }

            constexpr auto cmp(const void* _data) const -> bool {
                return !std::memcmp(data, _data, size);
            }

            u8 data[20]{};
            u8 size{};
        };

        constexpr auto cmp_cond(u32 inst) -> bool {
            const auto type = inst >> 24;
            return type == 0x6B || // cmp w0, w1
                   type == 0xF1;   // cmp x0, #0x1
        }

        constexpr auto bl_cond(u32 inst) -> bool {
            const auto type = inst >> 24;
            return type == 0x25 ||
                   type == 0x94 ||
                   type == 0x97;
        }

        constexpr auto tbz_cond(u32 inst) -> bool {
            return ((inst >> 24) & 0x7F) == 0x36;
        }

        constexpr PatchData ret0_patch_data{ "0xE0031F2A" };
        constexpr PatchData nop_patch_data{ "0x1F2003D5" };
        constexpr PatchData cmp_patch_data{ "0x00" };

        constexpr auto ret0_applied(const u8* data) -> bool { return ret0_patch_data.cmp(data); }
        constexpr auto nop_applied(const u8* data) -> bool { return nop_patch_data.cmp(data); }
        constexpr auto cmp_applied(const u8* data) -> bool { return cmp_patch_data.cmp(data); }

        struct Pattern {
            const char* name;
            const PatternData byte_pattern;
            const s32 inst_offset;
            const s32 patch_offset;
            bool (*const cond)(u32 inst);
            bool (*const applied)(const u8* data);
            const u32 min_fw_ver{FW_VER_ANY};
            const u32 max_fw_ver{FW_VER_ANY};
        };

        constexpr Pattern fs_patterns[] = {
            // noacidsigchk: moved to loader in 10.0.0
            { "noacidsigchk_1.0.0-9.2.0", "0xC8FE4739", -24, 0, bl_cond, ret0_applied, FW_VER_ANY, MAKEHOSVERSION(9,2,0) },
            { "noacidsigchk_1.0.0-9.2.0", "0x0210911F000072", -5, 0, bl_cond, ret0_applied, FW_VER_ANY, MAKEHOSVERSION(9,2,0) },
            // noncasigchk
            { "noncasigchk_1.0.0-3.0.2", "0x88..42..58", -4, 0, tbz_cond, nop_applied, MAKEHOSVERSION(1,0,0), MAKEHOSVERSION(3,0,2) },
            { "noncasigchk_4.0.0-16.1.0", "0x1E4839....00......0054", -17, 0, tbz_cond, nop_applied, MAKEHOSVERSION(4,0,0), MAKEHOSVERSION(16,1,0) },
            { "noncasigchk_17.0.0+", "0x0694....00..42..0091", -18, 0, tbz_cond, nop_applied, MAKEHOSVERSION(17,0,0), FW_VER_ANY },
            // nocntchk
            { "nocntchk_1.0.0-18.1.0", "0x40F9........081C00121F05", 2, 0, bl_cond, ret0_applied, MAKEHOSVERSION(1,0,0), MAKEHOSVERSION(18,1,0) },
            { "nocntchk_19.0.0+", "0x40F9............40B9091C", 2, 0, bl_cond, ret0_applied, MAKEHOSVERSION(19,0,0), FW_VER_ANY },
        };

        constexpr Pattern ldr_patterns[] = {
            // 1F00016B (cmp w0, w1) patched to 1F00006B (cmp w0, w0)
            { "noacidsigchk_10.0.0+", "0x009401C0BE121F00", 6, 2, cmp_cond, cmp_applied, FW_VER_ANY },
        };

        enum class PatternState { NotFound, Skipped, Patched, Unpatched };

        auto versionApplies(u32 fw_version, u32 min_fw_ver, u32 max_fw_ver) -> bool {
            if (min_fw_ver && min_fw_ver > fw_version) return false;
            if (max_fw_ver && max_fw_ver < fw_version) return false;
            return true;
        }

        void matchPatterns(std::span<const u8> data, std::span<const Pattern> patterns, PatternState* states) {
            for (size_t p = 0; p < patterns.size(); p++) {
                const auto& pat = patterns[p];

                if (states[p] != PatternState::NotFound) {
                    continue;
                }

                if (data.size() < pat.byte_pattern.size) {
                    continue;
                }

                for (size_t i = 0; i <= data.size() - pat.byte_pattern.size; i++) {
                    u8 count{};
                    while (count < pat.byte_pattern.size) {
                        if (pat.byte_pattern.data[count] != data[i + count] && pat.byte_pattern.data[count] != REGEX_SKIP) {
                            break;
                        }
                        count++;
                    }

                    if (count != pat.byte_pattern.size) {
                        continue;
                    }

                    const s64 inst_offset = static_cast<s64>(i) + pat.inst_offset;
                    if (inst_offset < 0 || static_cast<u64>(inst_offset) + sizeof(u32) > data.size()) {
                        continue;
                    }

                    u32 inst{};
                    std::memcpy(&inst, data.data() + inst_offset, sizeof(inst));

                    const s64 patch_offset = inst_offset + pat.patch_offset;
                    if (patch_offset < 0 || static_cast<u64>(patch_offset) + sizeof(u32) > data.size()) {
                        continue;
                    }

                    if (pat.applied(data.data() + patch_offset)) {
                        states[p] = PatternState::Patched;
                        break;
                    }

                    if (pat.cond(inst)) {
                        states[p] = PatternState::Unpatched;
                        break;
                    }
                }
            }
        }

        void scanAttachedProcess(Handle handle, std::span<const Pattern> patterns, PatternState* states) {
            std::vector<u8> data;
            u64 addr{};

            for (;;) {
                MemoryInfo mem_info{};
                u32 page_info{};

                if (R_FAILED(svcQueryDebugProcessMemory(&mem_info, &page_info, handle, addr))) {
                    break;
                }

                addr = mem_info.addr + mem_info.size;
                if (!addr) {
                    break;
                }

                if (!mem_info.size || mem_info.size > MAX_REGION_SIZE) continue;
                if ((mem_info.perm & Perm_Rx) != Perm_Rx) continue;
                if ((mem_info.type & 0xFF) != MemType_CodeStatic) continue;

                data.resize(mem_info.size);
                if (R_FAILED(svcReadDebugProcessMemory(data.data(), handle, mem_info.addr, data.size()))) {
                    continue;
                }

                matchPatterns(data, patterns, states);
            }
        }

        auto scanProgram(u64 program_id, std::span<const Pattern> patterns, PatternState* states) -> bool {
            Handle handle{};

            u64 pids[0x50]{};
            s32 process_count{};

            if (R_FAILED(svcGetProcessList(&process_count, pids, static_cast<u32>(std::size(pids))))) {
                return false;
            }

            u64 self_pid = UINT64_MAX;
            svcGetProcessId(&self_pid, CUR_PROCESS_HANDLE);

            for (s32 i = 0; i < process_count; i++) {
                if (pids[i] == self_pid) {
                    continue;
                }

                if (R_FAILED(svcDebugActiveProcess(&handle, pids[i]))) {
                    continue;
                }

                DebugEventInfo event_info{};
                if (R_FAILED(svcGetDebugEvent(&event_info, handle)) ||
                    event_info.info.create_process.program_id != program_id) {
                    svcCloseHandle(handle);
                    continue;
                }

                scanAttachedProcess(handle, patterns, states);
                svcCloseHandle(handle);
                return true;
            }

            return false;
        }

        auto verdictFor(std::span<const Pattern> patterns, const PatternState* states) -> SigPatchScanResult {
            bool any_applicable = false;

            for (size_t p = 0; p < patterns.size(); p++) {
                if (states[p] == PatternState::Skipped) {
                    continue;
                }
                any_applicable = true;
                if (states[p] != PatternState::Patched) {
                    return SigPatchScanResult::Unpatched;
                }
            }

            return any_applicable ? SigPatchScanResult::Patched : SigPatchScanResult::Unavailable;
        }

        auto runScan() -> SigPatchScanResult {
            const u32 fw_version = hosversionGet();
            if (!fw_version) {
                return SigPatchScanResult::Unavailable;
            }

            PatternState fs_states[std::size(fs_patterns)]{};
            PatternState ldr_states[std::size(ldr_patterns)]{};

            for (size_t p = 0; p < std::size(fs_patterns); p++) {
                if (!versionApplies(fw_version, fs_patterns[p].min_fw_ver, fs_patterns[p].max_fw_ver)) {
                    fs_states[p] = PatternState::Skipped;
                }
            }

            if (!scanProgram(FS_PROGRAM_ID, fs_patterns, fs_states)) {
                return SigPatchScanResult::Unavailable;
            }

            const auto fs_verdict = verdictFor(fs_patterns, fs_states);
            if (fs_verdict != SigPatchScanResult::Patched) {
                return fs_verdict;
            }

            if (fw_version < MAKEHOSVERSION(10,0,0)) {
                return SigPatchScanResult::Patched;
            }

            for (size_t p = 0; p < std::size(ldr_patterns); p++) {
                if (!versionApplies(fw_version, ldr_patterns[p].min_fw_ver, ldr_patterns[p].max_fw_ver)) {
                    ldr_states[p] = PatternState::Skipped;
                }
            }

            if (!scanProgram(LDR_PROGRAM_ID, ldr_patterns, ldr_states)) {
                return SigPatchScanResult::Unavailable;
            }

            return verdictFor(ldr_patterns, ldr_states);
        }
    }

    SigPatchScanResult scanSigPatchesInMemory() {
        static bool cached = false;
        static SigPatchScanResult result = SigPatchScanResult::Unavailable;

        if (!cached) {
            result = runScan();
            cached = true;
        }

        return result;
    }
}

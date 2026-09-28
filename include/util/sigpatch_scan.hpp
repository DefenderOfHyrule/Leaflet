#pragma once

namespace inst::util {
    enum class SigPatchScanResult {
        Patched,
        Unpatched,
        Unavailable,
    };

    SigPatchScanResult scanSigPatchesInMemory();
}

#pragma once
// ============================================================================
// SSEAntiDebug.h -- Anti-debugging utilities (cross-platform)
// ============================================================================

#include <cstdint>

namespace SimpleScriptEngine {
namespace Crypto {
namespace AntiDebug {

enum class DetectionLevel {
    None    = 0,
    Suspect = 1,
    Likely  = 2,
    Certain = 3
};

struct DetectionResult {
    DetectionLevel level        = DetectionLevel::None;
    const char*    triggerName  = nullptr;
    uint32_t       score        = 0;
};

// Combined detection
DetectionResult detect(uint32_t threshold = 30);

// Quick check
bool isDebuggerPresent();

// Individual checks
bool checkDebuggerFlag();
bool checkPEBFlag();
bool checkNtGlobalFlag();
bool checkHardwareBreakpoints();
bool checkTimingAnomaly(uint64_t thresholdUs = 100000);
bool checkSoftwareBreakpoints(const uint8_t* codeStart, size_t codeSize);

} // namespace AntiDebug
} // namespace Crypto
} // namespace SimpleScriptEngine

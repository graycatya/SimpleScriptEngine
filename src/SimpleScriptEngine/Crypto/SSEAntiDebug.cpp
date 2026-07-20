// ============================================================================
// SSEAntiDebug.cpp — 反调试检测实现 (跨平台)
// ============================================================================

#include "SSEAntiDebug.h"

#if defined(_WIN32) || defined(_WIN64)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#elif defined(__APPLE__)
    #include <sys/sysctl.h>
    #include <sys/types.h>
    #include <unistd.h>
#elif defined(__linux__)
    #include <sys/ptrace.h>
    #include <unistd.h>
#endif

#include <chrono>
#include <cstring>

namespace SimpleScriptEngine {
namespace Crypto {
namespace AntiDebug {

// ================================================================
// Detection Level 配置
// ================================================================

#if defined(SIMPLESCRIPTENGINE_ANTI_DEBUG_HEAVY)
    static constexpr bool ENABLE_HEAVY = true;
    static constexpr bool ENABLE_LIGHT = true;
#elif defined(SIMPLESCRIPTENGINE_ANTI_DEBUG_LIGHT)
    static constexpr bool ENABLE_HEAVY = false;
    static constexpr bool ENABLE_LIGHT = true;
#else
    static constexpr bool ENABLE_HEAVY = false;
    static constexpr bool ENABLE_LIGHT = false;
#endif

// ================================================================
// 检测 1: IsDebuggerPresent / ptrace
// ================================================================

bool checkDebuggerFlag() {
#if defined(_WIN32) || defined(_WIN64)
    return IsDebuggerPresent() != 0;

#elif defined(__linux__)
    // ptrace(PTRACE_TRACEME, 0, 0, 0) 在调试器下会失败
    if (ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) == -1) {
        return true;  // 已经被调试
    }
    // 成功: 告诉内核停止 trace，恢复正常
    ptrace(PTRACE_DETACH, 0, nullptr, nullptr);
    return false;

#elif defined(__APPLE__)
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
    struct kinfo_proc info;
    size_t size = sizeof(info);
    info.kp_proc.p_flag = 0;

    if (sysctl(mib, 4, &info, &size, nullptr, 0) == 0) {
        return (info.kp_proc.p_flag & P_TRACED) != 0;
    }
    return false;

#else
    return false;
#endif
}

// ================================================================
// 检测 2: PEB.BeingDebugged / TracerPid
// ================================================================

bool checkPEBFlag() {
#if defined(_WIN32) || defined(_WIN64)
    // 通过 PEB 检查 BeingDebugged 标志
    // 使用内联汇编或 __readfsbyte
#if defined(_MSC_VER)
    __try {
        // PEB 在 FS:[0x30] (32-bit) 或 GS:[0x60] (64-bit)
#ifdef _WIN64
        uint8_t beingDebugged = *(uint8_t*)(__readgsqword(0x60) + 2);
#else
        uint8_t beingDebugged = *(uint8_t*)(__readfsdword(0x30) + 2);
#endif
        return beingDebugged != 0;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#else
    // GCC/MinGW 使用内联汇编
    uint8_t beingDebugged = 0;
#ifdef _WIN64
    __asm__ volatile(
        "movq %%gs:0x60, %%rax\n\t"
        "movb 2(%%rax), %0"
        : "=r"(beingDebugged) :: "rax");
#else
    __asm__ volatile(
        "movl %%fs:0x30, %%eax\n\t"
        "movb 2(%%eax), %0"
        : "=r"(beingDebugged) :: "eax");
#endif
    return beingDebugged != 0;
#endif

#elif defined(__linux__)
    // 检查 /proc/self/status 中的 TracerPid
    FILE* f = fopen("/proc/self/status", "r");
    if (!f) return false;

    char line[256];
    bool traced = false;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "TracerPid:", 10) == 0) {
            int pid = 0;
            sscanf(line + 10, "%d", &pid);
            traced = (pid != 0);
            break;
        }
    }
    fclose(f);
    return traced;

#else
    return false;
#endif
}

// ================================================================
// 检测 3: NtGlobalFlag (Windows)
// ================================================================

bool checkNtGlobalFlag() {
#if defined(_WIN32) || defined(_WIN64)
    // NtGlobalFlag 在 PEB + 0x68 (32-bit) / PEB + 0xBC (64-bit)
    // 调试器会设置 FLG_HEAP_ENABLE_TAIL_CHECK (0x10) 等标志
    // 正常进程此值为 0，调试下通常为 0x70

#if defined(_MSC_VER)
    __try {
#ifdef _WIN64
        uint32_t ngt = *(uint32_t*)(__readgsqword(0x60) + 0xBC);
#else
        uint32_t ngt = *(uint32_t*)(__readfsdword(0x30) + 0x68);
#endif
        // 常见调试标志: 0x10 | 0x20 | 0x40 = 0x70
        return (ngt & 0x70) != 0;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#else
    return false; // GCC 上简化处理
#endif

#else
    return false;
#endif
}

// ================================================================
// 检测 4: 硬件断点 (DR0-DR3)
// ================================================================

bool checkHardwareBreakpoints() {
#if defined(_WIN32) || defined(_WIN64)
    // 使用 GetThreadContext 读取调试寄存器
    HANDLE hThread = GetCurrentThread();
    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;

    if (GetThreadContext(hThread, &ctx)) {
        // 检查 DR0-DR3 是否有非零值 (硬件断点地址)
        if (ctx.Dr0 != 0 || ctx.Dr1 != 0 || ctx.Dr2 != 0 || ctx.Dr3 != 0) {
            return true;
        }
    }
    return false;

#elif defined(__linux__) || defined(__APPLE__)
    // x86/x64 上可通过类似方式检查
    // 简化：检查 DR7 是否启用
    // 注：需要 signal handler 或 ptrace，此处简化返回 false
    return false;

#else
    return false;
#endif
}

// ================================================================
// 检测 5: 时间异常
// ================================================================

bool checkTimingAnomaly(uint64_t thresholdUs) {
    using namespace std::chrono;

    auto t1 = high_resolution_clock::now();
    // 执行一些简单计算 (防止被优化掉)
    volatile int x = 0;
    for (volatile int i = 0; i < 1000; ++i) {
        x += i;
    }
    auto t2 = high_resolution_clock::now();

    auto elapsed = duration_cast<microseconds>(t2 - t1).count();

    // 正常执行 1000 次循环在微秒级
    // 如果有调试器单步或 int3 断点，时间会异常大
    (void)x; // 防止 unused 警告
    return elapsed > static_cast<decltype(elapsed)>(thresholdUs);
}

// ================================================================
// 检测 6: 软件断点 (INT3 / 0xCC) 扫描
// ================================================================

bool checkSoftwareBreakpoints(const uint8_t* codeStart, size_t codeSize) {
    if (!codeStart || codeSize == 0) return false;

    size_t int3count = 0;
    for (size_t i = 0; i < codeSize; ++i) {
        if (codeStart[i] == 0xCC) {
            ++int3count;
            if (int3count > 3) {  // 正常代码中 int3 极少
                return true;
            }
        }
    }
    return false;
}

// ================================================================
// 综合检测
// ================================================================

DetectionResult detect(uint32_t threshold) {
    DetectionResult result;

    // 等级 1: 基本检测
    if (ENABLE_LIGHT) {
        if (checkDebuggerFlag()) {
            result.score += 40;
            result.triggerName = "DebuggerFlag";
        }
        if (checkPEBFlag()) {
            result.score += 35;
            if (!result.triggerName) result.triggerName = "PEBFlag";
        }
        if (checkTimingAnomaly(200000)) {
            result.score += 15;
            if (!result.triggerName) result.triggerName = "TimingAnomaly";
        }
    }

    // 等级 2: 深度检测
    if (ENABLE_HEAVY) {
        if (checkNtGlobalFlag()) {
            result.score += 20;
            if (!result.triggerName) result.triggerName = "NtGlobalFlag";
        }
        if (checkHardwareBreakpoints()) {
            result.score += 30;
            if (!result.triggerName) result.triggerName = "HardwareBP";
        }
    }

    // 评估等级
    if (result.score >= 70) {
        result.level = DetectionLevel::Certain;
    } else if (result.score >= 50) {
        result.level = DetectionLevel::Likely;
    } else if (result.score >= static_cast<uint32_t>(threshold)) {
        result.level = DetectionLevel::Suspect;
    } else {
        result.level = DetectionLevel::None;
    }

    return result;
}

bool isDebuggerPresent() {
    auto r = detect(25);
    return r.level >= DetectionLevel::Suspect;
}

} // namespace AntiDebug
} // namespace Crypto
} // namespace SimpleScriptEngine

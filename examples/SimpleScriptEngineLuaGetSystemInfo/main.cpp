// ============================================================================
// SimpleScriptEngine — LuaJIT FFI System Information Demo (Cross-Platform)
// ============================================================================
// Demonstrates:
//   1. Using LuaJITEngine to load and execute Lua scripts
//   2. Lua scripts calling OS-native APIs via FFI
//       - Windows : kernel32.dll / user32.dll / ntdll.dll
//       - macOS   : libSystem (sysctl, POSIX, etc.)
//   3. Collecting CPU, memory, OS version, screen resolution, uptime, battery, etc.
// ============================================================================

#include "SimpleScriptManager.h"
#include "Engines/LuaJITEngine.h"

#include <iostream>
#include <string>

int main() {
    std::cout << "\n"
              << "  +------------------------------------------------------+\n"
              << "  |   SimpleScriptEngine -- LuaJIT FFI System Info Demo  |\n"
              << "  +------------------------------------------------------+\n"
              << std::endl;

    SimpleScriptEngine::LuaJITEngine engine;

    // ---- Initialize ----
    std::cout << "[C++] Initializing " << engine.engineName()
              << " v" << engine.engineVersion() << " ..." << std::endl;

    if (!engine.initialize()) {
        std::cerr << "[C++] ERROR: Engine initialization failed!" << std::endl;
        return 1;
    }
    std::cout << "[C++] Engine initialized successfully" << std::endl;

    // ---- Select script by platform ----
#if defined(_WIN32) || defined(_WIN64)
    std::string scriptPath = "sysinfo-windows.lua";
#elif defined(__APPLE__)
    std::string scriptPath = "sysinfo-macos.lua";
#else
    std::string scriptPath = "sysinfo-linux.lua";  // Linux (POSIX + /proc + /sys)
#endif

    std::cout << "[C++] Loading script: " << scriptPath << std::endl;

    if (!engine.executeFile(scriptPath)) {
        std::cerr << "[C++] ERROR: Script execution failed!" << std::endl;
        engine.shutdown();
        return 1;
    }

    // ---- Shutdown ----
    engine.shutdown();
    std::cout << "\n[C++] Engine shutdown complete." << std::endl;

    return 0;
}

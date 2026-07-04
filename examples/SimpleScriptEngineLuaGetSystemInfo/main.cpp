// ============================================================================
// SimpleScriptEngine — LuaJIT FFI Windows System Information Demo
// ============================================================================
// Demonstrates:
//   1. Using LuaJITEngine to load and execute Lua scripts
//   2. Lua scripts calling kernel32.dll / user32.dll / ntdll.dll via FFI
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

    // ---- Load and execute Lua script from file ----
    std::string scriptPath = "sysinfo-windows.lua";
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

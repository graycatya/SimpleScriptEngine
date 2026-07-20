// ============================================================================
// SSEPakDemo -- .ssep package loader with custom key support
// ============================================================================

#include "SimpleScriptManager.h"
#include "Crypto/SSEPakLoader.h"
#include "Crypto/SSEKeyManager.h"

#ifdef SIMPLESCRIPTENGINE_ENABLE_LUAJIT
#include "Engines/LuaJITEngine.h"
#endif
#ifdef SIMPLESCRIPTENGINE_ENABLE_QUICKJS
#include "Engines/QuickJSEngine.h"
#endif

#include <iostream>
#include <memory>
#include <string>

namespace sse  = SimpleScriptEngine;
namespace ssec = SimpleScriptEngine::Crypto;
#ifdef SIMPLESCRIPTENGINE_ENABLE_LUAJIT
using sse::LuaJITEngine;
#endif
#ifdef SIMPLESCRIPTENGINE_ENABLE_QUICKJS
using sse::QuickJSEngine;
#endif
using sse::SimpleScriptManager;
using ssec::KeyManager;
using ssec::LoadResult;
using ssec::SSEPakLoader;
using ssec::SSEPakReader;

int main(int argc, char* argv[])
{
    const char* pakPath = "scripts.ssep";
    if (argc > 1) {
        pakPath = argv[1];
    }

    std::cout << "=== SSEP Pak Demo ===\n";
    std::cout << "Loading: " << pakPath << "\n\n";

    // ---- Step 1: Peek header to detect custom key ----
    bool needsKey = SSEPakReader::pakNeedsCustomKey(pakPath);
    std::cout << "Custom key required: " << (needsKey ? "YES" : "no (built-in)") << "\n";

    if (needsKey) {
        // In a real UI app, this would show a dialog box:
        //   QInputDialog::getText(nullptr, "Key Required", "Enter decryption key:");
        std::cout << "\nEnter key (hex, e.g. a1b2c3d4...): ";
        std::string hexKey;
        std::getline(std::cin, hexKey);

        if (hexKey.empty()) {
            std::cout << "No key provided, aborting.\n";
            return 1;
        }

        if (!KeyManager::instance().setCustomKeyHex(hexKey.c_str())) {
            std::cout << "Invalid key format (need >= 32 hex chars).\n";
            return 1;
        }
        std::cout << "Key accepted, attempting to load...\n\n";
    }

    // ---- Step 2: Register engines ----
    SimpleScriptManager& mgr = SimpleScriptManager::instance();

#ifdef SIMPLESCRIPTENGINE_ENABLE_LUAJIT
    mgr.registerEngine(std::make_unique<LuaJITEngine>());
    std::cout << "[OK] LuaJIT\n";
#endif
#ifdef SIMPLESCRIPTENGINE_ENABLE_QUICKJS
    mgr.registerEngine(std::make_unique<QuickJSEngine>());
    std::cout << "[OK] QuickJS\n";
#endif

    mgr.initializeAll();

    // ---- Step 3: Load encrypted package ----
    SSEPakLoader loader;
    LoadResult result = loader.loadPak(pakPath, mgr);

    std::cout << "\n--- Result ---\n";
    std::cout << "Total:   " << result.totalFiles << "\n";
    std::cout << "Success: " << result.successFiles << "\n";
    std::cout << "Failed:  " << result.failedFiles << "\n";

    if (!result.failures.empty()) {
        std::cout << "\nFailures:\n";
        for (auto& f : result.failures) {
            std::cout << "  " << f.first << " -> " << f.second << "\n";
        }
    }

    mgr.shutdownAll();
    return result.failedFiles > 0 ? 1 : 0;
}

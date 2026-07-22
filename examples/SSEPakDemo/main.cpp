// ============================================================================
// SSEPakDemo — 加密脚本包加载 (不需要 include Crypto 头文件)
// ============================================================================

#include "SimpleScriptManager.h"

#ifdef SIMPLESCRIPTENGINE_ENABLE_PAK_PROTECTION
#include "Crypto/SSEKeyManager.h"
#include "Crypto/SSEPakReader.h"
#endif

#ifdef SIMPLESCRIPTENGINE_ENABLE_LUAJIT
#include "Engines/LuaJITEngine.h"
#endif
#ifdef SIMPLESCRIPTENGINE_ENABLE_QUICKJS
#include "Engines/QuickJSEngine.h"
#endif

#include <iostream>
#include <memory>
#include <string>

namespace sse = SimpleScriptEngine;
#ifdef SIMPLESCRIPTENGINE_ENABLE_LUAJIT
using sse::LuaJITEngine;
#endif
#ifdef SIMPLESCRIPTENGINE_ENABLE_QUICKJS
using sse::QuickJSEngine;
#endif
using sse::SimpleScriptManager;

int main(int argc, char* argv[])
{
    const char* pakPath = "scripts.ssep";
    if (argc > 1) {
        pakPath = argv[1];
    }

    std::cout << "=== SSEP Pak Demo ===\n";
    std::cout << "Loading: " << pakPath << "\n\n";

    // ---- 检测是否需要自定义密钥 ----
#ifdef SIMPLESCRIPTENGINE_ENABLE_PAK_PROTECTION
    using namespace SimpleScriptEngine::Crypto;

    bool needsKey = SSEPakReader::pakNeedsCustomKey(pakPath);
    std::cout << "Custom key required: " << (needsKey ? "YES" : "no") << "\n";

    if (needsKey) {
        std::cout << "\nEnter key (hex): ";
        std::string hexKey;
        std::getline(std::cin, hexKey);
        if (hexKey.empty()) {
            std::cout << "No key provided, aborting.\n";
            return 1;
        }
        if (!KeyManager::instance().setCustomKeyHex(hexKey.c_str())) {
            std::cout << "Invalid key format.\n";
            return 1;
        }
        std::cout << "Key accepted.\n\n";
    }
#endif

    // ---- 注册引擎 ----
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

    // ---- 加载加密包 (Crypto 完全不可见) ----
    bool ok = mgr.executePak(pakPath);

    std::cout << "\n--- Result ---\n";
    std::cout << "Status: " << (ok ? "SUCCESS" : "FAILED") << "\n";

    mgr.shutdownAll();
    return ok ? 0 : 1;
}

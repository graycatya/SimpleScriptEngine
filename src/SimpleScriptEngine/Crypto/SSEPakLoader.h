#pragma once
// ============================================================================
// SSEPakLoader.h -- SimpleScriptEngine integration loader
// ============================================================================

#include "../SimpleScriptEngine.h"
#include "../SimpleScriptManager.h"
#include "SSEPakReader.h"

#include <string>
#include <vector>

namespace SimpleScriptEngine {
namespace Crypto {

struct LoadResult {
    uint32_t totalFiles   = 0;
    uint32_t successFiles = 0;
    uint32_t failedFiles  = 0;

    std::vector<std::pair<std::string, std::string>> failures;
};

class SSEPakLoader {
public:
    SSEPakLoader();
    ~SSEPakLoader();

    // Load all scripts from .ssep into a single engine
    LoadResult loadPak(const std::string& pakPath, SimpleScriptEngine& engine);

    // Load all scripts from .ssep into manager (auto-routed by extension)
    LoadResult loadPak(const std::string& pakPath, SimpleScriptManager& manager);

    // Low-level: open pak without executing
    bool openPak(const std::string& pakPath);

    // Execute a decrypted buffer into an engine
    static bool executeBuffer(SimpleScriptEngine& engine,
                              EngineType entryType,
                              const uint8_t* data, size_t size);

    SSEPakReader& getReader() { return reader_; }

    void close();

private:
    SSEPakReader reader_;

    static EngineType engineNameToType(const std::string& name);
    static EngineType extensionToType(const std::string& ext);
};

} // namespace Crypto
} // namespace SimpleScriptEngine

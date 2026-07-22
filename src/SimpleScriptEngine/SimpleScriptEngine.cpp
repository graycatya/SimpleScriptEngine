// ============================================================================
// SimpleScriptEngine — PIMPL implementation (Crypto hidden in .cpp)
// ============================================================================

#include "SimpleScriptEngine.h"

#ifdef SIMPLESCRIPTENGINE_ENABLE_PAK_PROTECTION
    #include "Crypto/SSEPakLoader.h"
#endif

namespace SimpleScriptEngine {

class SimpleScriptEngine::Impl {
public:
    ErrorCallback errorCallback;
};

SimpleScriptEngine::SimpleScriptEngine()
    : impl_(std::make_unique<Impl>()) {}

SimpleScriptEngine::~SimpleScriptEngine() = default;

bool SimpleScriptEngine::executeBuffer(const uint8_t* data, size_t size) {
    return executeString(std::string(reinterpret_cast<const char*>(data), size));
}

void SimpleScriptEngine::setErrorCallback(ErrorCallback cb) {
    impl_->errorCallback = std::move(cb);
}

SimpleScriptEngine::ErrorCallback SimpleScriptEngine::errorCallback() const {
    return impl_->errorCallback;
}

void SimpleScriptEngine::reportError(const std::string& engineName,
                                      const std::string& msg) {
    if (impl_->errorCallback) {
        impl_->errorCallback(engineName, msg);
    }
}

// ================================================================
// Encrypted package execution (hidden implementation)
// ================================================================

bool SimpleScriptEngine::executePak(const std::string& pakPath) {
#ifdef SIMPLESCRIPTENGINE_ENABLE_PAK_PROTECTION
    Crypto::SSEPakLoader loader;
    Crypto::LoadResult result = loader.loadPak(pakPath, *this);
    return result.successFiles > 0;
#else
    (void)pakPath;
    return false;
#endif
}

} // namespace SimpleScriptEngine

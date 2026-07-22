#pragma once
// ============================================================================
// SimpleScriptEngine — Abstract base class (ABI-stable, PIMPL)
// ============================================================================
// All virtual methods preserved. errorCallback_ moved into PIMPL.
// executePak() added for encrypted package support.
// ============================================================================

#include "SimpleScriptValue.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace SimpleScriptEngine {

class SimpleScriptEngine {
public:
    using ErrorCallback = std::function<void(const std::string& engineName,
                                              const std::string& errorMsg)>;

    SimpleScriptEngine();
    virtual ~SimpleScriptEngine();

    // ---- lifecycle ----
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual bool isInitialized() const = 0;

    // ---- script execution ----
    virtual const char* engineName()    const = 0;
    virtual const char* engineVersion() const = 0;
    virtual const char* fileExtension() const = 0;

    virtual bool executeString(const std::string& code) = 0;
    virtual bool executeFile(const std::string& path) = 0;
    virtual bool executeBuffer(const uint8_t* data, size_t size);

    virtual SimpleScriptValue eval(const std::string& expr) = 0;
    virtual SimpleScriptValue call(const std::string& func,
                                   const std::vector<SimpleScriptValue>& args = {}) = 0;

    virtual bool hasFunction(const std::string& name) = 0;

    virtual void setGlobal(const std::string& name, const SimpleScriptValue& value) = 0;
    virtual SimpleScriptValue getGlobal(const std::string& name) = 0;
    virtual void registerFunction(const std::string& name, ScriptFunction func) = 0;

    // ---- encrypted package ----
    virtual bool executePak(const std::string& pakPath);

    // ---- error callback (implementation in PIMPL) ----
    void setErrorCallback(ErrorCallback cb);
    ErrorCallback errorCallback() const;

protected:
    void reportError(const std::string& engineName, const std::string& msg);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace SimpleScriptEngine

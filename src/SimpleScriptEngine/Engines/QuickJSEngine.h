#pragma once

#include "../SimpleScriptEngine.h"

#include <memory>

// ============================================================================
// QuickJSEngine — QuickJS 脚本引擎适配器 (PIMPL)
// ============================================================================
//
// 使用 QuickJS C API (JSRuntime / JSContext)
// 轻量级 ES2020 兼容 JavaScript 引擎
//
// 特性：
//   - 完整 ES2020 支持
//   - 极小内存占用
//   - 可嵌入式设计
// ============================================================================

namespace SimpleScriptEngine {

class QuickJSEngine : public SimpleScriptEngine {
public:
    QuickJSEngine();
    ~QuickJSEngine() override;

    QuickJSEngine(QuickJSEngine&& other) noexcept;
    QuickJSEngine& operator=(QuickJSEngine&& other) noexcept;

    // ---- SimpleScriptEngine 接口 ----

    const char* engineName()    const noexcept override { return "QuickJS"; }
    const char* engineVersion() const noexcept override;
    const char* fileExtension() const noexcept override { return ".js"; }

    bool initialize() override;
    void shutdown() override;
    bool isInitialized() const noexcept override;

    bool executeString(const std::string& code) override;
    bool executeFile(const std::string& path) override;
    SimpleScriptValue eval(const std::string& expr) override;

    SimpleScriptValue call(const std::string& func,
                           const std::vector<SimpleScriptValue>& args = {}) override;

    bool hasFunction(const std::string& name) override;

    void setGlobal(const std::string& name, const SimpleScriptValue& value) override;
    SimpleScriptValue getGlobal(const std::string& name) override;

    void registerFunction(const std::string& name, ScriptFunction func) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace SimpleScriptEngine

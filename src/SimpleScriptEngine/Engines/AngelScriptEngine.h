#pragma once

#include "../SimpleScriptEngine.h"

#include <memory>

// ============================================================================
// AngelScriptEngine — AngelScript 脚本引擎适配器 (PIMPL)
// ============================================================================
//
// 使用 AngelScript C++ API (asIScriptEngine)
// 静态类型脚本语言，语法接近 C/C++
//
// 特性：
//   - 静态类型，编译期检查
//   - 原生 C/C++ 调用约定
//   - 广泛用于游戏行业
// ============================================================================

namespace SimpleScriptEngine {

class AngelScriptEngine : public SimpleScriptEngine {
public:
    AngelScriptEngine();
    ~AngelScriptEngine() override;

    AngelScriptEngine(AngelScriptEngine&& other) noexcept;
    AngelScriptEngine& operator=(AngelScriptEngine&& other) noexcept;

    // ---- SimpleScriptEngine 接口 ----

    const char* engineName()    const noexcept override { return "AngelScript"; }
    const char* engineVersion() const noexcept override;
    const char* fileExtension() const noexcept override { return ".as"; }

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

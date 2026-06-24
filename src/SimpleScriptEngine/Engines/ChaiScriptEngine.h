#pragma once

#include "../SimpleScriptEngine.h"

#include <memory>

// ============================================================================
// ChaiScriptEngine — ChaiScript 脚本引擎适配器 (PIMPL)
// ============================================================================
//
// 使用 ChaiScript C++ 头文件库
// 原生 C++ 脚本语言，与 C++ 无缝互操作
//
// 特性：
//   - 原生 C++ 语法风格
//   - header-only，零依赖
//   - 直接访问 C++ 类型和函数
// ============================================================================

namespace SimpleScriptEngine {

class ChaiScriptEngine : public SimpleScriptEngine {
public:
    ChaiScriptEngine();
    ~ChaiScriptEngine() override;

    ChaiScriptEngine(ChaiScriptEngine&& other) noexcept;
    ChaiScriptEngine& operator=(ChaiScriptEngine&& other) noexcept;

    // ---- SimpleScriptEngine 接口 ----

    const char* engineName()    const noexcept override { return "ChaiScript"; }
    const char* engineVersion() const noexcept override;
    const char* fileExtension() const noexcept override { return ".chai"; }

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

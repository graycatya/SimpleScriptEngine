#pragma once

#include "../SimpleScriptEngine.h"

#include <memory>

// ============================================================================
// LuaJITEngine — LuaJIT 脚本引擎适配器 (PIMPL)
// ============================================================================
//
// 使用 LuaJIT 2.1 C API (lua_State)
// PIMPL 确保 luajit 头文件不泄露到公共接口
//
// 特性：
//   - JIT 编译，性能接近原生
//   - 完整 Lua 5.1 兼容
//   - FFI 支持直接调用 C 函数
// ============================================================================

namespace SimpleScriptEngine {

class LuaJITEngine : public SimpleScriptEngine {
public:
    LuaJITEngine();
    ~LuaJITEngine() override;

    // 移动语义（PIMPL 需要）
    LuaJITEngine(LuaJITEngine&& other) noexcept;
    LuaJITEngine& operator=(LuaJITEngine&& other) noexcept;

    // ---- SimpleScriptEngine 接口 ----

    const char* engineName()    const noexcept override { return "LuaJIT"; }
    const char* engineVersion() const noexcept override;
    const char* fileExtension() const noexcept override { return ".lua"; }

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

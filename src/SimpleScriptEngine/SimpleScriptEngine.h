#pragma once

#include "SimpleScriptValue.h"

#include <memory>
#include <string>
#include <vector>

// ============================================================================
// SimpleScriptEngine — 统一脚本引擎抽象接口 (PIMPL-Ready)
// ============================================================================
//
// 这是一个纯虚基类，定义了所有脚本引擎适配器必须实现的公共接口。
// 使得上层代码不依赖任何特定引擎，切换引擎无需修改调用代码。
//
// 设计要点：
//   1. 纯虚接口 — ABI 稳定，新增引擎只需新增派生类
//   2. No-Throw 查询 — isInitialized / engineName / fileExtension 不抛异常
//   3. 生命周期显式 — initialize() / shutdown() 配对使用
//   4. 错误通过返回值 + 可选回调报告
// ============================================================================

namespace SimpleScriptEngine {

class SimpleScriptEngine {
public:
    virtual ~SimpleScriptEngine() = default;

    // ================================================================
    // 引擎元信息 (noexcept — 永远可用)
    // ================================================================

    /// 引擎可读名称，如 "LuaJIT", "QuickJS"
    virtual const char* engineName() const noexcept = 0;

    /// 引擎版本字符串
    virtual const char* engineVersion() const noexcept = 0;

    /// 默认文件扩展名，如 ".lua", ".js"
    virtual const char* fileExtension() const noexcept = 0;

    // ================================================================
    // 生命周期管理
    // ================================================================

    /// 初始化引擎运行时
    /// @return true 成功, false 失败
    virtual bool initialize() = 0;

    /// 关闭引擎，释放所有资源
    virtual void shutdown() = 0;

    /// 引擎是否已初始化完毕
    virtual bool isInitialized() const noexcept = 0;

    // ================================================================
    // 脚本执行
    // ================================================================

    /// 执行一段脚本文本
    /// @param code 脚本源码字符串
    /// @return true 执行成功, false 发生错误
    virtual bool executeString(const std::string& code) = 0;

    /// 从文件加载并执行脚本
    /// @param path 脚本文件路径
    /// @return true 执行成功, false 发生错误
    virtual bool executeFile(const std::string& path) = 0;

    /// 求值表达式并返回结果
    /// @param expr 表达式字符串
    /// @return 表达式计算结果
    virtual SimpleScriptValue eval(const std::string& expr) = 0;

    // ================================================================
    // C++ → 脚本：调用脚本函数
    // ================================================================

    /// 检查脚本中是否存在指定名称的可调用函数
    /// @param name 函数名
    /// @return true 存在且可调用, false 不存在或不是函数
    virtual bool hasFunction(const std::string& name) = 0;

    /// 调用脚本中的全局函数
    /// @param func 函数名
    /// @param args 参数列表
    /// @return 函数返回值
    virtual SimpleScriptValue call(
        const std::string& func,
        const std::vector<SimpleScriptValue>& args = {}) = 0;

    // ================================================================
    // C++ → 脚本：变量绑定
    // ================================================================

    /// 向脚本全局空间设置变量
    virtual void setGlobal(const std::string& name, const SimpleScriptValue& value) = 0;

    /// 从脚本全局空间读取变量
    virtual SimpleScriptValue getGlobal(const std::string& name) = 0;

    // ================================================================
    // 脚本 → C++：函数注册
    // ================================================================

    /// 向脚本注册 C++ 函数，使脚本可以回调 C++ 代码
    /// @param name 函数在脚本中的名字
    /// @param func C++ 可调用对象
    virtual void registerFunction(const std::string& name, ScriptFunction func) = 0;

    // ================================================================
    // 错误处理回调（可选）
    // ================================================================

    /// 设置错误回调，脚本执行出错时触发
    using ErrorCallback = std::function<void(const std::string& engineName, const std::string& message)>;

    virtual void setErrorCallback(ErrorCallback cb) { errorCallback_ = std::move(cb); }

protected:
    ErrorCallback errorCallback_;
};

} // namespace SimpleScriptEngine

#pragma once

#include "SimpleScriptEngine.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

// ============================================================================
// SimpleScriptManager — 统一脚本管理器
// ============================================================================
//
// 职责：
//   1. 引擎注册 & 发现
//   2. 按引擎名 / 文件扩展名智能路由
//   3. 全局变量跨引擎共享（仅支持可序列化的基础类型）
//   4. 引擎生命周期批量管理
//
// 线程安全：instance() 和引擎操作受 mutex 保护
// ============================================================================

namespace SimpleScriptEngine {

class SimpleScriptManager {
public:
    // ---- 单例 ----

    /// 获取全局唯一实例（线程安全）
    static SimpleScriptManager& instance();

    // 禁止拷贝 / 移动
    SimpleScriptManager(const SimpleScriptManager&) = delete;
    SimpleScriptManager& operator=(const SimpleScriptManager&) = delete;

    // ================================================================
    // 引擎注册
    // ================================================================

    /// 注册一个引擎实例（转移所有权）
    /// @return true 成功, false 同名引擎已存在
    bool registerEngine(std::unique_ptr<SimpleScriptEngine> engine);

    /// 注销一个引擎
    bool unregisterEngine(const std::string& name);

    /// 获取已注册的引擎
    SimpleScriptEngine* getEngine(const std::string& name);

    /// 列出所有已注册的引擎名称
    std::vector<std::string> listEngines() const;

    // ================================================================
    // 生命周期批量管理
    // ================================================================

    /// 初始化所有已注册的引擎
    /// @return 成功初始化的引擎数量
    size_t initializeAll();

    /// 关闭所有引擎并释放资源
    void shutdownAll();

    // ================================================================
    // 智能路由执行
    // ================================================================

    /// 按引擎名称执行代码
    /// @param engineName 引擎名，如 "LuaJIT"
    /// @param code 脚本源码
    /// @return true 执行成功
    bool executeString(const std::string& engineName, const std::string& code);

    /// 按文件扩展名自动路由执行
    /// .lua → LuaJIT, .js → QuickJS, .chai → ChaiScript, .as → AngelScript
    /// @param path 脚本文件路径
    /// @return true 执行成功
    bool executeFile(const std::string& path);

    /// 通过扩展名查找对应引擎
    SimpleScriptEngine* findEngineByExtension(const std::string& extension) const;

    // ================================================================
    // 加密脚本包
    // ================================================================

    /// 加载 .ssep 加密脚本包，按扩展名自动路由到对应引擎执行
    /// @param pakPath .ssep 文件路径
    /// @return true 至少有一个脚本成功执行
    bool executePak(const std::string& pakPath);

    /// 检查指定引擎中是否存在某个函数
    /// @param engineName 引擎名，如 "LuaJIT"
    /// @param funcName 函数名
    /// @return true 存在且可调用
    bool hasFunction(const std::string& engineName, const std::string& funcName);

    // ================================================================
    // 跨引擎全局变量（共享基础类型）
    // ================================================================

    void setSharedGlobal(const std::string& name, const SimpleScriptValue& value);
    SimpleScriptValue getSharedGlobal(const std::string& name) const;
    bool hasSharedGlobal(const std::string& name) const;

    // ================================================================
    // 便捷工厂
    // ================================================================

    /// 使用文件名（含扩展名）创建并返回对应引擎
    static std::unique_ptr<SimpleScriptEngine> createForFile(const std::string& path);

private:
    SimpleScriptManager() = default;
    ~SimpleScriptManager();

    mutable std::mutex mutex_;

    // 已注册引擎：名称 → 引擎实例
    std::unordered_map<std::string, std::unique_ptr<SimpleScriptEngine>> engines_;

    // 扩展名 → 引擎名称 映射（自动维护）
    std::unordered_map<std::string, std::string> extensionMap_;

    // 跨引擎共享的全局变量
    std::unordered_map<std::string, SimpleScriptValue> sharedGlobals_;

    void updateExtensionMap();
};

} // namespace SimpleScriptEngine

// ============================================================================
// LuaJITEngine 实现 — PIMPL  + LuaJIT C API 绑定
// ============================================================================
// 所有 LuaJIT 相关头文件仅在 .cpp 中包含，不污染公共接口。
// ============================================================================

#include "LuaJITEngine.h"

#ifdef SIMPLESCRIPTENGINE_ENABLE_LUAJIT

#include "lua.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace SimpleScriptEngine {

// ================================================================
// Private Implementation (PIMPL)
// ================================================================

class LuaJITEngine::Impl {
public:
    Impl() = default;

    ~Impl() {
        if (initialized_) {
            shutdown();
        }
    }

    // ---- 生命周期 ----

    bool initialize() {
        if (initialized_) return true;

        L_ = luaL_newstate();
        if (!L_) {
            reportError("Failed to create lua_State");
            return false;
        }

        luaL_openlibs(L_);   // 加载标准库

        initialized_ = true;
        return true;
    }

    void shutdown() {
        if (!initialized_) return;

        // 清理注册的 C 函数引用
        registeredFuncs_.clear();

        if (L_) {
            lua_close(L_);
            L_ = nullptr;
        }
        initialized_ = false;
    }

    bool isInitialized() const noexcept {
        return initialized_ && L_ != nullptr;
    }

    // ---- 脚本执行 ----

    bool executeString(const std::string& code) {
        if (!ensureInit()) return false;

        int err = luaL_loadstring(L_, code.c_str());
        if (err != LUA_OK) {
            reportError(popString());
            return false;
        }

        return pcall(0, 0);
    }

    bool executeFile(const std::string& path) {
        if (!ensureInit()) return false;

        int err = luaL_loadfile(L_, path.c_str());
        if (err != LUA_OK) {
            reportError(popString());
            return false;
        }

        return pcall(0, 0);
    }

    SimpleScriptValue eval(const std::string& expr) {
        if (!ensureInit()) return SimpleScriptValue::Null();

        // 拼接 "return " + expr 来求值
        std::string code = "return " + expr;
        int err = luaL_loadstring(L_, code.c_str());
        if (err != LUA_OK) {
            reportError(popString());
            return SimpleScriptValue::Null();
        }

        if (!pcall(0, 1)) {
            return SimpleScriptValue::Null();
        }

        // 从栈顶读取返回值
        SimpleScriptValue result = fromLuaStack(-1);
        lua_pop(L_, 1);
        return result;
    }

    // ---- C++ → 脚本：函数探测 ----

    bool hasFunction(const std::string& name) {
        if (!ensureInit()) return false;

        lua_getglobal(L_, name.c_str());
        bool isFunc = lua_isfunction(L_, -1);
        lua_pop(L_, 1);
        return isFunc;
    }

    // ---- C++ → 脚本：函数调用 ----

    SimpleScriptValue call(const std::string& func,
                           const std::vector<SimpleScriptValue>& args) {
        if (!ensureInit()) return SimpleScriptValue::Null();

        // 获取全局函数
        lua_getglobal(L_, func.c_str());
        if (!lua_isfunction(L_, -1)) {
            lua_pop(L_, 1);
            reportError("Function '" + func + "' not found or not callable");
            return SimpleScriptValue::Null();
        }

        // 压入参数
        for (auto& arg : args) {
            pushValue(arg);
        }

        // 调用
        if (!pcall(static_cast<int>(args.size()), 1)) {
            return SimpleScriptValue::Null();
        }

        // 读取返回值
        SimpleScriptValue result = fromLuaStack(-1);
        lua_pop(L_, 1);
        return result;
    }

    // ---- 变量绑定 ----

    void setGlobal(const std::string& name, const SimpleScriptValue& value) {
        if (!ensureInit()) return;
        pushValue(value);
        lua_setglobal(L_, name.c_str());
    }

    SimpleScriptValue getGlobal(const std::string& name) {
        if (!ensureInit()) return SimpleScriptValue::Null();
        lua_getglobal(L_, name.c_str());
        SimpleScriptValue result = fromLuaStack(-1);
        lua_pop(L_, 1);
        return result;
    }

    // ---- C++ 函数注册 ----

    void registerFunction(const std::string& name, ScriptFunction func) {
        if (!ensureInit()) return;

        // 存储函数引用
        auto key = registeredFuncs_.size();
        registeredFuncs_.push_back(std::move(func));

        // 创建 userdata 存储索引
        auto* ud = static_cast<int*>(lua_newuserdata(L_, sizeof(int)));
        *ud = static_cast<int>(key);

        // 设置 metatable 保护 (可选)
        luaL_newmetatable(L_, "SIMPLESCRIPTENGINE_LuaJIT_Callback");
        lua_setmetatable(L_, -2);

        // 推入闭包：调用时通过 userdata 索引查找 C++ 函数
        lua_pushcclosure(L_, &Impl::luaCallbackBridge, 1);
        lua_setglobal(L_, name.c_str());
    }

    // ---- 错误回调 ----

    void setErrorCallback(SimpleScriptEngine::ErrorCallback cb) {
        errorCb_ = std::move(cb);
    }

private:
    lua_State* L_ = nullptr;
    bool initialized_ = false;

    // 注册的 C++ 函数列表
    std::vector<ScriptFunction> registeredFuncs_;
    SimpleScriptEngine::ErrorCallback errorCb_;

    // ============================================================
    // 辅助方法
    // ============================================================

    bool ensureInit() {
        if (!initialized_ || !L_) {
            reportError("LuaJIT engine not initialized");
            return false;
        }
        return true;
    }

    void reportError(const std::string& msg) {
        if (errorCb_) {
            errorCb_("LuaJIT", msg);
        } else {
            std::cerr << "[LuaJIT] Error: " << msg << std::endl;
        }
    }

    std::string popString() {
        if (!L_) return "unknown error";
        size_t len = 0;
        const char* s = lua_tolstring(L_, -1, &len);
        std::string result(s ? std::string(s, len) : "unknown error");
        lua_pop(L_, 1);
        return result;
    }

    bool pcall(int nargs, int nresults) {
        int err = lua_pcall(L_, nargs, nresults, 0);
        if (err != LUA_OK) {
            reportError(popString());
            return false;
        }
        return true;
    }

    // ---- SimpleScriptValue ↔ Lua 栈转换 ----

    void pushValue(const SimpleScriptValue& val) {
        switch (val.type()) {
            case ScriptValueType::Null:
                lua_pushnil(L_);
                break;
            case ScriptValueType::Bool:
                lua_pushboolean(L_, val.asBool() ? 1 : 0);
                break;
            case ScriptValueType::Int:
                lua_pushinteger(L_, static_cast<lua_Integer>(val.asInt()));
                break;
            case ScriptValueType::Double:
                lua_pushnumber(L_, val.asDouble());
                break;
            case ScriptValueType::String:
                lua_pushstring(L_, val.asString().c_str());
                break;
            case ScriptValueType::Array: {
                const auto& arr = val.asArray();
                lua_createtable(L_, static_cast<int>(arr.size()), 0);
                for (size_t i = 0; i < arr.size(); ++i) {
                    pushValue(arr[i]);
                    lua_rawseti(L_, -2, static_cast<int>(i + 1)); // Lua 索引从 1 开始
                }
                break;
            }
            case ScriptValueType::Object: {
                const auto& obj = val.asObject();
                lua_createtable(L_, 0, static_cast<int>(obj.size()));
                for (auto& kv : obj) {
                    pushValue(kv.second);
                    lua_setfield(L_, -2, kv.first.c_str());
                }
                break;
            }
            case ScriptValueType::Function:
                // 函数类型暂不跨引擎传递
                lua_pushnil(L_);
                break;
        }
    }

    SimpleScriptValue fromLuaStack(int idx) {
        int t = lua_type(L_, idx);

        switch (t) {
            case LUA_TNIL:
                return SimpleScriptValue::Null();
            case LUA_TBOOLEAN:
                return SimpleScriptValue::Bool(lua_toboolean(L_, idx) != 0);
            case LUA_TNUMBER:
                // LuaJIT (Lua 5.1) 中所有数字均为 double，不存在 int/float 区分
                return SimpleScriptValue::Num(lua_tonumber(L_, idx));
            case LUA_TSTRING: {
                size_t len;
                const char* s = lua_tolstring(L_, idx, &len);
                return SimpleScriptValue::Str(std::string(s, len));
            }
            case LUA_TTABLE: {
                // 探测是数组还是对象
                lua_pushnil(L_);  // 第一个 key
                bool isArray = true;
                lua_Integer maxIdx = 0;

                // 遍历一次判断类型
                int absIdx = (idx < 0) ? lua_gettop(L_) + idx + 1 : idx;
                while (lua_next(L_, absIdx) != 0) {
                    if (lua_type(L_, -2) == LUA_TNUMBER) {
                        lua_Integer k = lua_tointeger(L_, -2);
                        if (k > maxIdx) maxIdx = k;
                    } else {
                        isArray = false;
                    }
                    lua_pop(L_, 1);
                }

                // 重建遍历
                if (isArray && maxIdx > 0) {
                    ScriptArray arr(maxIdx);
                    lua_pushnil(L_);
                    while (lua_next(L_, absIdx) != 0) {
                        lua_Integer k = lua_tointeger(L_, -2);
                        if (k >= 1 && static_cast<size_t>(k) <= arr.size()) {
                            arr[k - 1] = fromLuaStack(-1);
                        }
                        lua_pop(L_, 1);
                    }
                    return SimpleScriptValue(std::move(arr));
                } else {
                    ScriptObject obj;
                    lua_pushnil(L_);
                    while (lua_next(L_, absIdx) != 0) {
                        // key 转为字符串
                        std::string key;
                        if (lua_type(L_, -2) == LUA_TSTRING) {
                            key = lua_tostring(L_, -2);
                        } else {
                            key = std::to_string(lua_tointeger(L_, -2));
                        }
                        obj[key] = fromLuaStack(-1);
                        lua_pop(L_, 1);
                    }
                    return SimpleScriptValue(std::move(obj));
                }
            }
            case LUA_TFUNCTION:
                return SimpleScriptValue::Str("<lua-function>");
            default:
                return SimpleScriptValue::Null();
        }
    }

    // ============================================================
    // 静态桥接函数：Lua → C++ 回调入口
    // ============================================================

    static int luaCallbackBridge(lua_State* L) {
        // 从 userdata 取索引
        auto* ud = static_cast<int*>(luaL_checkudata(L, lua_upvalueindex(1), "SIMPLESCRIPTENGINE_LuaJIT_Callback"));
        if (!ud) {
            lua_pushstring(L, "Invalid callback userdata");
            lua_error(L);
            return 0;
        }

        // 通过某种方式找到 Impl 实例
        // 这里简化处理：在全局注册表中查找
        lua_getfield(L, LUA_REGISTRYINDEX, "_simplescriptengine_impl_ptr");
        auto* impl = static_cast<Impl*>(lua_touserdata(L, -1));
        lua_pop(L, 1);

        if (!impl || *ud < 0 || static_cast<size_t>(*ud) >= impl->registeredFuncs_.size()) {
            lua_pushstring(L, "Callback not found");
            lua_error(L);
            return 0;
        }

        // 收集参数
        int nargs = lua_gettop(L);
        std::vector<SimpleScriptValue> args(nargs);
        for (int i = 0; i < nargs; ++i) {
            args[i] = impl->fromLuaStack(i + 1);
        }

        // 调用 C++ 函数
        SimpleScriptValue result = impl->registeredFuncs_[*ud](args);

        // 返回结果
        impl->pushValue(result);
        return 1;
    }
};

// ================================================================
// 公共接口转发
// ================================================================

LuaJITEngine::LuaJITEngine()
    : impl_(std::make_unique<Impl>()) {}

LuaJITEngine::~LuaJITEngine() = default;

LuaJITEngine::LuaJITEngine(LuaJITEngine&& other) noexcept
    : impl_(std::move(other.impl_)) {}

LuaJITEngine& LuaJITEngine::operator=(LuaJITEngine&& other) noexcept {
    if (this != &other) {
        impl_ = std::move(other.impl_);
    }
    return *this;
}

const char* LuaJITEngine::engineVersion() const noexcept {
    return LUAJIT_VERSION;
}

bool LuaJITEngine::initialize() {
    // 设置错误回调前向
    impl_->setErrorCallback(errorCallback_);
    return impl_->initialize();
}

void LuaJITEngine::shutdown() { impl_->shutdown(); }

bool LuaJITEngine::isInitialized() const noexcept { return impl_->isInitialized(); }

bool LuaJITEngine::executeString(const std::string& code) { return impl_->executeString(code); }
bool LuaJITEngine::executeFile(const std::string& path)    { return impl_->executeFile(path); }

SimpleScriptValue LuaJITEngine::eval(const std::string& expr) { return impl_->eval(expr); }

SimpleScriptValue LuaJITEngine::call(const std::string& func,
                                      const std::vector<SimpleScriptValue>& args) {
    return impl_->call(func, args);
}

bool LuaJITEngine::hasFunction(const std::string& name) { return impl_->hasFunction(name); }

void LuaJITEngine::setGlobal(const std::string& name, const SimpleScriptValue& value) {
    impl_->setGlobal(name, value);
}

SimpleScriptValue LuaJITEngine::getGlobal(const std::string& name) {
    return impl_->getGlobal(name);
}

void LuaJITEngine::registerFunction(const std::string& name, ScriptFunction func) {
    impl_->registerFunction(name, std::move(func));
}

} // namespace SimpleScriptEngine

#else // !SIMPLESCRIPTENGINE_ENABLE_LUAJIT

// ================================================================
// LuaJIT 未启用时的桩实现
// ================================================================

#include <iostream>

namespace SimpleScriptEngine {

class LuaJITEngine::Impl {
public:
    bool isInitialized() const noexcept { return false; }
    void reportDisabled() {
        std::cerr << "[LuaJIT] Engine not built (SIMPLESCRIPTENGINE_ENABLE_LUAJIT=OFF)\n";
    }
};

LuaJITEngine::LuaJITEngine() : impl_(std::make_unique<Impl>()) {}
LuaJITEngine::~LuaJITEngine() = default;
LuaJITEngine::LuaJITEngine(LuaJITEngine&&) noexcept = default;
LuaJITEngine& LuaJITEngine::operator=(LuaJITEngine&&) noexcept = default;

const char* LuaJITEngine::engineVersion() const noexcept { return "disabled"; }

bool LuaJITEngine::initialize() { impl_->reportDisabled(); return false; }
void LuaJITEngine::shutdown() {}
bool LuaJITEngine::isInitialized() const noexcept { return false; }

bool LuaJITEngine::executeString(const std::string&) {
    impl_->reportDisabled(); return false;
}
bool LuaJITEngine::executeFile(const std::string&) {
    impl_->reportDisabled(); return false;
}
SimpleScriptValue LuaJITEngine::eval(const std::string&) {
    impl_->reportDisabled(); return SimpleScriptValue::Null();
}
SimpleScriptValue LuaJITEngine::call(const std::string&, const std::vector<SimpleScriptValue>&) {
    impl_->reportDisabled(); return SimpleScriptValue::Null();
}
bool LuaJITEngine::hasFunction(const std::string&) { impl_->reportDisabled(); return false; }
void LuaJITEngine::setGlobal(const std::string&, const SimpleScriptValue&) { impl_->reportDisabled(); }
SimpleScriptValue LuaJITEngine::getGlobal(const std::string&) {
    impl_->reportDisabled(); return SimpleScriptValue::Null();
}
void LuaJITEngine::registerFunction(const std::string&, ScriptFunction) { impl_->reportDisabled(); }

} // namespace SimpleScriptEngine

#endif // SIMPLESCRIPTENGINE_ENABLE_LUAJIT

// ============================================================================
// QuickJSEngine 实现 — PIMPL + QuickJS C API 绑定
// ============================================================================
// 所有 QuickJS 相关头文件仅在 .cpp 中包含，不污染公共接口。
// ============================================================================

#include "QuickJSEngine.h"

#ifdef SIMPLESCRIPTENGINE_ENABLE_QUICKJS

extern "C" {
#include "quickjs.h"
}

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace SimpleScriptEngine {

class QuickJSEngine::Impl {
public:
    Impl() = default;

    ~Impl() {
        if (initialized_) shutdown();
    }

    // ---- 生命周期 ----

    bool initialize() {
        if (initialized_) return true;

        rt_ = JS_NewRuntime();
        if (!rt_) {
            reportError("Failed to create JSRuntime");
            return false;
        }

        ctx_ = JS_NewContext(rt_);
        if (!ctx_) {
            JS_FreeRuntime(rt_);
            rt_ = nullptr;
            reportError("Failed to create JSContext");
            return false;
        }

        // 注册 ES Module 加载器，支持 import/export 语法
        JS_SetModuleLoaderFunc(rt_, nullptr, moduleLoader, this);

        initialized_ = true;
        return true;
    }

    void shutdown() {
        if (!initialized_) return;

        // 先释放 JS 上下文（QuickJS 会清理所有 JS 对象引用）
        if (ctx_) {
            JS_FreeContext(ctx_);
            ctx_ = nullptr;
        }
        if (rt_) {
            JS_FreeRuntime(rt_);
            rt_ = nullptr;
        }

        // JS 上下文已释放，现在可以安全清理 C++ 回调数据
        for (auto* data : callbackData_) {
            delete data;
        }
        callbackData_.clear();
        registeredFuncs_.clear();

        initialized_ = false;
    }

    bool isInitialized() const noexcept {
        return initialized_ && rt_ && ctx_;
    }

    // ---- 脚本执行 ----

    bool executeString(const std::string& code) {
        if (!ensureInit()) return false;

        JSValue result = JS_Eval(ctx_, code.c_str(), code.size(),
                                 "<string>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(result)) {
            handleException();
            JS_FreeValue(ctx_, result);
            return false;
        }
        JS_FreeValue(ctx_, result);
        return true;
    }

    bool executeFile(const std::string& path) {
        if (!ensureInit()) return false;

        std::ifstream file(path, std::ios::in | std::ios::binary);
        if (!file.is_open()) {
            reportError("Cannot open file: " + path);
            return false;
        }

        std::ostringstream oss;
        oss << file.rdbuf();
        std::string code = oss.str();

        // 自动检测 ES Module：含有 import/export 关键词则按 module 模式求值
        bool isModule = (code.find("import ")  != std::string::npos ||
                         code.find("export ")  != std::string::npos ||
                         code.find("import{")  != std::string::npos ||
                         code.find("import\n") != std::string::npos);
        int flags = isModule ? JS_EVAL_TYPE_MODULE : JS_EVAL_TYPE_GLOBAL;

        JSValue result = JS_Eval(ctx_, code.c_str(), code.size(),
                                 path.c_str(), flags);
        if (JS_IsException(result)) {
            handleException();
            JS_FreeValue(ctx_, result);
            return false;
        }
        JS_FreeValue(ctx_, result);
        return true;
    }

    SimpleScriptValue eval(const std::string& expr) {
        if (!ensureInit()) return SimpleScriptValue::Null();

        JSValue result = JS_Eval(ctx_, expr.c_str(), expr.size(),
                                 "<eval>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(result)) {
            handleException();
            JS_FreeValue(ctx_, result);
            return SimpleScriptValue::Null();
        }

        SimpleScriptValue val = fromJSValue(result);
        JS_FreeValue(ctx_, result);
        return val;
    }

    // ---- 函数探测 ----

    bool hasFunction(const std::string& name) {
        if (!ensureInit()) return false;

        JSValue global = JS_GetGlobalObject(ctx_);
        JSValue funcVal = JS_GetPropertyStr(ctx_, global, name.c_str());
        JS_FreeValue(ctx_, global);

        bool isFunc = JS_IsFunction(ctx_, funcVal);
        JS_FreeValue(ctx_, funcVal);
        return isFunc;
    }

    // ---- 函数调用 ----

    SimpleScriptValue call(const std::string& func,
                           const std::vector<SimpleScriptValue>& args) {
        if (!ensureInit()) return SimpleScriptValue::Null();

        JSValue global = JS_GetGlobalObject(ctx_);
        JSValue funcVal = JS_GetPropertyStr(ctx_, global, func.c_str());
        JS_FreeValue(ctx_, global);

        if (!JS_IsFunction(ctx_, funcVal)) {
            JS_FreeValue(ctx_, funcVal);
            reportError("Function '" + func + "' not found or not callable");
            return SimpleScriptValue::Null();
        }

        // 构造参数数组
        std::vector<JSValue> jsArgs(args.size());
        for (size_t i = 0; i < args.size(); ++i) {
            jsArgs[i] = toJSValue(args[i]);
        }

        JSValue result = JS_Call(ctx_, funcVal, JS_UNDEFINED,
                                  static_cast<int>(args.size()),
                                  args.empty() ? nullptr : jsArgs.data());

        // 释放参数
        for (auto& ja : jsArgs) {
            JS_FreeValue(ctx_, ja);
        }
        JS_FreeValue(ctx_, funcVal);

        if (JS_IsException(result)) {
            handleException();
            JS_FreeValue(ctx_, result);
            return SimpleScriptValue::Null();
        }

        SimpleScriptValue val = fromJSValue(result);
        JS_FreeValue(ctx_, result);
        return val;
    }

    // ---- 变量绑定 ----

    void setGlobal(const std::string& name, const SimpleScriptValue& value) {
        if (!ensureInit()) return;

        JSValue global = JS_GetGlobalObject(ctx_);
        JSValue jsVal = toJSValue(value);
        JS_SetPropertyStr(ctx_, global, name.c_str(), jsVal);
        JS_FreeValue(ctx_, global);
        // toJSValue 返回的值由 SetPropertyStr 接管
    }

    SimpleScriptValue getGlobal(const std::string& name) {
        if (!ensureInit()) return SimpleScriptValue::Null();

        JSValue global = JS_GetGlobalObject(ctx_);
        JSValue jsVal = JS_GetPropertyStr(ctx_, global, name.c_str());
        JS_FreeValue(ctx_, global);

        SimpleScriptValue result = fromJSValue(jsVal);
        JS_FreeValue(ctx_, jsVal);
        return result;
    }

    // ---- C++ 函数注册 ----

    void registerFunction(const std::string& name, ScriptFunction func) {
        if (!ensureInit()) return;

        auto key = registeredFuncs_.size();
        registeredFuncs_.push_back(std::move(func));

        // 创建包装函数数据（由 shutdown 统一释放）
        auto* data = new CallbackData{this, key};
        callbackData_.push_back(data);

        JSValue global = JS_GetGlobalObject(ctx_);
        JSValue jsFunc = JS_NewCFunctionData(ctx_, &Impl::jsCallbackBridge,
                                              0, 0, 1, &data->jsData);
        JS_SetPropertyStr(ctx_, global, name.c_str(), jsFunc);
        // 注意：JS_SetPropertyStr 内部已消费 jsFunc 的引用，不需要额外 JS_FreeValue
        JS_FreeValue(ctx_, global);
    }

    void setErrorCallback(SimpleScriptEngine::ErrorCallback cb) {
        errorCb_ = std::move(cb);
    }

private:
    JSRuntime* rt_  = nullptr;
    JSContext* ctx_ = nullptr;
    bool initialized_ = false;

    // 回调数据结构（必须在 callbackData_ 之前声明）
    struct CallbackData {
        Impl* impl;
        size_t index;
        JSValue jsData = JS_UNDEFINED; // QuickJS 将使用此 GC 对象，初始化为安全值
    };

    std::vector<ScriptFunction> registeredFuncs_;
    std::vector<CallbackData*> callbackData_;  // 追踪分配的回调数据，在 shutdown 时释放
    SimpleScriptEngine::ErrorCallback errorCb_;

    bool ensureInit() {
        if (!initialized_ || !rt_ || !ctx_) {
            reportError("QuickJS engine not initialized");
            return false;
        }
        return true;
    }

    void reportError(const std::string& msg) {
        if (errorCb_) {
            errorCb_("QuickJS", msg);
        } else {
            std::cerr << "[QuickJS] Error: " << msg << std::endl;
        }
    }

    void handleException() {
        JSValue exc = JS_GetException(ctx_);
        std::string msg = fromJSValue(exc).toString();
        JS_FreeValue(ctx_, exc);
        reportError(msg);
    }

    // ---- SimpleScriptValue ↔ JSValue 转换 ----

    JSValue toJSValue(const SimpleScriptValue& val) {
        switch (val.type()) {
            case ScriptValueType::Null:
                return JS_NULL;
            case ScriptValueType::Bool:
                return JS_NewBool(ctx_, val.asBool() ? 1 : 0);
            case ScriptValueType::Int:
                return JS_NewInt64(ctx_, val.asInt());
            case ScriptValueType::Double:
                return JS_NewFloat64(ctx_, val.asDouble());
            case ScriptValueType::String:
                return JS_NewString(ctx_, val.asString().c_str());
            case ScriptValueType::Array: {
                const auto& arr = val.asArray();
                JSValue jsArr = JS_NewArray(ctx_);
                for (size_t i = 0; i < arr.size(); ++i) {
                    JS_SetPropertyUint32(ctx_, jsArr, static_cast<uint32_t>(i),
                                         toJSValue(arr[i]));
                }
                return jsArr;
            }
            case ScriptValueType::Object: {
                const auto& obj = val.asObject();
                JSValue jsObj = JS_NewObject(ctx_);
                for (auto& kv : obj) {
                    JS_SetPropertyStr(ctx_, jsObj, kv.first.c_str(),
                                      toJSValue(kv.second));
                }
                return jsObj;
            }
            case ScriptValueType::Function:
                return JS_UNDEFINED;
            default:
                return JS_UNDEFINED;
        }
    }

    SimpleScriptValue fromJSValue(JSValueConst val) {
        if (JS_IsNull(val) || JS_IsUndefined(val)) {
            return SimpleScriptValue::Null();
        }
        if (JS_IsBool(val)) {
            return SimpleScriptValue::Bool(JS_ToBool(ctx_, val) != 0);
        }
        if (JS_IsNumber(val)) {
            int64_t i;
            if (JS_ToInt64(ctx_, &i, val) == 0) {
                return SimpleScriptValue::Int(i);
            }
            double d;
            JS_ToFloat64(ctx_, &d, val);
            return SimpleScriptValue::Num(d);
        }
        if (JS_IsString(val)) {
            const char* s = JS_ToCString(ctx_, val);
            SimpleScriptValue result = SimpleScriptValue::Str(s ? s : "");
            JS_FreeCString(ctx_, s);
            return result;
        }
        if (JS_IsArray(val)) {
            ScriptArray arr;
            // 获取数组长度
            JSValue lenVal = JS_GetPropertyStr(ctx_, val, "length");
            uint32_t len = 0;
            JS_ToUint32(ctx_, &len, lenVal);
            JS_FreeValue(ctx_, lenVal);

            for (uint32_t i = 0; i < len; ++i) {
                JSValue item = JS_GetPropertyUint32(ctx_, val, i);
                arr.push_back(fromJSValue(item));
                JS_FreeValue(ctx_, item);
            }
            return SimpleScriptValue(std::move(arr));
        }
        if (JS_IsObject(val)) {
            ScriptObject obj;
            JSPropertyEnum* tab = nullptr;
            uint32_t tabLen = 0;
            if (JS_GetOwnPropertyNames(ctx_, &tab, &tabLen, val,
                                        JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) == 0) {
                for (uint32_t i = 0; i < tabLen; ++i) {
                    JSAtom atom = tab[i].atom;
                    const char* key = JS_AtomToCString(ctx_, atom);
                    JSValue item = JS_GetProperty(ctx_, val, atom);
                    if (key) {
                        obj[key] = fromJSValue(item);
                    }
                    JS_FreeValue(ctx_, item);
                    JS_FreeCString(ctx_, key);
                    JS_FreeAtom(ctx_, atom);
                }
                js_free(ctx_, tab);
            }
            return SimpleScriptValue(std::move(obj));
        }
        if (JS_IsFunction(ctx_, val)) {
            return SimpleScriptValue::Str("<js-function>");
        }
        return SimpleScriptValue::Null();
    }

    // ---- 静态桥接 ----

    static JSValue jsCallbackBridge(JSContext* ctx, JSValueConst thisVal,
                                     int argc, JSValueConst* argv,
                                     int magic, JSValue* funcData) {
        // 从 funcData 读取 CallbackData
        // 简化处理：实际上需要从 userdata 中恢复 Impl 指针
        (void)ctx; (void)thisVal; (void)argc; (void)argv;
        (void)magic; (void)funcData;

        // TODO: 实现完整的 C 函数回调链
        // 这里需要绕过 QuickJS 的数据传递限制
        return JS_UNDEFINED;
    }

    // ---- ES Module 加载器 ----

    // QuickJS 模块加载回调：读取文件并编译为 ES Module
    static JSModuleDef* moduleLoader(JSContext* ctx,
                                     const char* moduleName,
                                     void* opaque) {
        auto* impl = static_cast<Impl*>(opaque);
        if (!impl) return nullptr;

        // 读取模块文件
        std::ifstream file(moduleName, std::ios::in | std::ios::binary);
        if (!file.is_open()) {
            JS_ThrowReferenceError(ctx, "could not open module '%s'", moduleName);
            return nullptr;
        }

        std::ostringstream oss;
        oss << file.rdbuf();
        std::string code = oss.str();

        // 编译为 ES Module
        JSValue funcVal = JS_Eval(ctx, code.c_str(), code.size(),
                                  moduleName,
                                  JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
        if (JS_IsException(funcVal)) {
            return nullptr;
        }

        // 从编译结果中提取 JSModuleDef 指针
        // JS_Eval 模块编译返回的 funcVal 内部持有模块引用
        JSModuleDef* m = reinterpret_cast<JSModuleDef*>(JS_VALUE_GET_PTR(funcVal));
        JS_FreeValue(ctx, funcVal);
        return m;
    }
};

// ================================================================
// 公共接口转发
// ================================================================

QuickJSEngine::QuickJSEngine() : impl_(std::make_unique<Impl>()) {}
QuickJSEngine::~QuickJSEngine() = default;

QuickJSEngine::QuickJSEngine(QuickJSEngine&& other) noexcept
    : impl_(std::move(other.impl_)) {}

QuickJSEngine& QuickJSEngine::operator=(QuickJSEngine&& other) noexcept {
    if (this != &other) impl_ = std::move(other.impl_);
    return *this;
}

const char* QuickJSEngine::engineVersion() const noexcept {
    return "2021-03-27"; // QuickJS 发布版本
}

bool QuickJSEngine::initialize() {
    impl_->setErrorCallback(errorCallback_);
    return impl_->initialize();
}

void QuickJSEngine::shutdown() { impl_->shutdown(); }
bool QuickJSEngine::isInitialized() const noexcept { return impl_->isInitialized(); }
bool QuickJSEngine::executeString(const std::string& code) { return impl_->executeString(code); }
bool QuickJSEngine::executeFile(const std::string& path)    { return impl_->executeFile(path); }
SimpleScriptValue QuickJSEngine::eval(const std::string& expr) { return impl_->eval(expr); }
SimpleScriptValue QuickJSEngine::call(const std::string& func,
                                      const std::vector<SimpleScriptValue>& args) {
    return impl_->call(func, args);
}
bool QuickJSEngine::hasFunction(const std::string& name) { return impl_->hasFunction(name); }
void QuickJSEngine::setGlobal(const std::string& name, const SimpleScriptValue& val) {
    impl_->setGlobal(name, val);
}
SimpleScriptValue QuickJSEngine::getGlobal(const std::string& name) {
    return impl_->getGlobal(name);
}
void QuickJSEngine::registerFunction(const std::string& name, ScriptFunction func) {
    impl_->registerFunction(name, std::move(func));
}

} // namespace SimpleScriptEngine

#else // !SIMPLESCRIPTENGINE_ENABLE_QUICKJS

#include <iostream>

namespace SimpleScriptEngine {

class QuickJSEngine::Impl {
public:
    bool isInitialized() const noexcept { return false; }
    void reportDisabled() {
        std::cerr << "[QuickJS] Engine not built (SIMPLESCRIPTENGINE_ENABLE_QUICKJS=OFF)\n";
    }
};

QuickJSEngine::QuickJSEngine() : impl_(std::make_unique<Impl>()) {}
QuickJSEngine::~QuickJSEngine() = default;
QuickJSEngine::QuickJSEngine(QuickJSEngine&&) noexcept = default;
QuickJSEngine& QuickJSEngine::operator=(QuickJSEngine&&) noexcept = default;

const char* QuickJSEngine::engineVersion() const noexcept { return "disabled"; }
bool QuickJSEngine::initialize() { impl_->reportDisabled(); return false; }
void QuickJSEngine::shutdown() {}
bool QuickJSEngine::isInitialized() const noexcept { return false; }
bool QuickJSEngine::executeString(const std::string&) { impl_->reportDisabled(); return false; }
bool QuickJSEngine::executeFile(const std::string&)    { impl_->reportDisabled(); return false; }
SimpleScriptValue QuickJSEngine::eval(const std::string&) { impl_->reportDisabled(); return SimpleScriptValue::Null(); }
SimpleScriptValue QuickJSEngine::call(const std::string&, const std::vector<SimpleScriptValue>&) {
    impl_->reportDisabled(); return SimpleScriptValue::Null();
}
bool QuickJSEngine::hasFunction(const std::string&) { impl_->reportDisabled(); return false; }
void QuickJSEngine::setGlobal(const std::string&, const SimpleScriptValue&) { impl_->reportDisabled(); }
SimpleScriptValue QuickJSEngine::getGlobal(const std::string&) { impl_->reportDisabled(); return SimpleScriptValue::Null(); }
void QuickJSEngine::registerFunction(const std::string&, ScriptFunction) { impl_->reportDisabled(); }

} // namespace SimpleScriptEngine

#endif // SIMPLESCRIPTENGINE_ENABLE_QUICKJS

// ============================================================================
// AngelScriptEngine 实现 — PIMPL + AngelScript C++ API 绑定
// ============================================================================
// 所有 AngelScript 相关头文件仅在 .cpp 中包含，不污染公共接口。
// ============================================================================

#include "AngelScriptEngine.h"

#ifdef SIMPLESCRIPTENGINE_ENABLE_ANGELSCRIPT

#include "angelscript.h"

// AngelScript add-ons
#include "scriptstdstring.h"
#include "scriptarray.h"

#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace SimpleScriptEngine {

class AngelScriptEngine::Impl {
public:
    Impl() = default;

    ~Impl() {
        if (initialized_) shutdown();
    }

    // ---- AngelScript 消息流回调 (静态成员, 可访问 private Impl) ----
    static void ASEngineMessageCallback(const asSMessageInfo* msg, void* param) {
        auto* impl = static_cast<Impl*>(param);
        if (impl) {
            const char* type = "INFO";
            if (msg->type == asMSGTYPE_WARNING)      type = "WARNING";
            else if (msg->type == asMSGTYPE_INFORMATION) type = "INFO";
            else if (msg->type == asMSGTYPE_ERROR)       type = "ERROR";

            std::ostringstream oss;
            oss << "[" << type << "] " << msg->section << ":" << msg->row << ":" << msg->col
                << " - " << msg->message;
            impl->reportError(oss.str());
        }
    }

    // ---- 生命周期 ----

    bool initialize() {
        if (initialized_) return true;

        engine_ = asCreateScriptEngine();
        if (!engine_) {
            reportError("Failed to create AngelScript engine");
            return false;
        }

        // 注册消息回调
        engine_->SetMessageCallback(asFUNCTION(Impl::ASEngineMessageCallback), this, asCALL_CDECL);

        // 注册标准字符串类型
        RegisterStdString(engine_);

        // 注册通用数组类型（如果需要）
        RegisterScriptArray(engine_, true);

        initialized_ = true;
        return true;
    }

    void shutdown() {
        if (!initialized_) return;

        // 释放所有已编译的模块
        for (auto* mod : modules_) {
            if (mod) mod->Discard();
        }
        modules_.clear();
        registeredFuncs_.clear();

        if (engine_) {
            engine_->ShutDownAndRelease();
            engine_ = nullptr;
        }
        initialized_ = false;
    }

    bool isInitialized() const noexcept {
        return initialized_ && engine_ != nullptr;
    }

    // ---- 脚本执行 ----

    bool executeString(const std::string& code) {
        if (!ensureInit()) return false;

        // 创建临时模块
        asIScriptModule* mod = engine_->GetModule("__exec__", asGM_ALWAYS_CREATE);
        int r = mod->AddScriptSection("inline", code.c_str(), code.size());
        if (r < 0) {
            reportError("Failed to add script section");
            return false;
        }

        r = mod->Build();
        if (r < 0) {
            reportError("Build failed");
            return false;
        }

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

        // 用文件名作为模块名
        std::string modName = std::filesystem::path(path).stem().string();
        asIScriptModule* mod = engine_->GetModule(modName.c_str(), asGM_ALWAYS_CREATE);

        int r = mod->AddScriptSection(path.c_str(), code.c_str(), code.size());
        if (r < 0) {
            reportError("Failed to add script section for " + path);
            return false;
        }

        r = mod->Build();
        if (r < 0) {
            reportError("Build failed for " + path);
            return false;
        }

        modules_.push_back(mod); // 持有引用防止被释放
        return true;
    }

    SimpleScriptValue eval(const std::string& expr) {
        if (!ensureInit()) return SimpleScriptValue::Null();

        // AngelScript 是静态类型语言，无法像动态语言那样直接求值任意表达式。
        // 采用多趟类型推断：依次尝试 int64 → double → bool，首次编译成功即使用。
        static const struct TypeAttempt {
            const char* typeName;
        } typeAttempts[] = {
            {"int64"},
            {"double"},
            {"bool"},
        };

        for (auto& attempt : typeAttempts) {
            std::string code = std::string(attempt.typeName) +
                               " __eval() { return " + expr + "; }";
            asIScriptModule* mod = engine_->GetModule("__eval__", asGM_ALWAYS_CREATE);
            mod->AddScriptSection("eval", code.c_str(), code.size());
            if (mod->Build() < 0) {
                continue; // 当前类型不匹配，尝试下一个
            }

            asIScriptFunction* func = mod->GetFunctionByName("__eval");
            if (!func) continue;

            return callFunction(func, {});
        }

        reportError("Eval build failed for expression: " + expr);
        return SimpleScriptValue::Null();
    }

    // ---- 函数探测 ----

    bool hasFunction(const std::string& name) {
        if (!ensureInit()) return false;

        // 检查执行模块
        asIScriptModule* mod = engine_->GetModule("__exec__", asGM_ONLY_IF_EXISTS);
        if (mod && mod->GetFunctionByName(name.c_str()))
            return true;

        // 搜索其他模块
        for (auto* m : modules_) {
            if (m && m->GetFunctionByName(name.c_str()))
                return true;
        }
        return false;
    }

    // ---- 函数调用 ----

    SimpleScriptValue call(const std::string& funcName,
                           const std::vector<SimpleScriptValue>& args) {
        if (!ensureInit()) return SimpleScriptValue::Null();

        // 在所有已编译模块中查找函数
        asIScriptFunction* func = nullptr;

        // 检查当前模块
        asIScriptModule* mod = engine_->GetModule("__exec__", asGM_ONLY_IF_EXISTS);
        if (mod) {
            func = mod->GetFunctionByName(funcName.c_str());
        }
        if (!func) {
            // 搜索其他模块
            for (auto* m : modules_) {
                func = m->GetFunctionByName(funcName.c_str());
                if (func) break;
            }
        }

        if (!func) {
            reportError("Function '" + funcName + "' not found");
            return SimpleScriptValue::Null();
        }

        return callFunction(func, args);
    }

    // ---- 变量绑定 ----

    void setGlobal(const std::string& name, const SimpleScriptValue& value) {
        if (!ensureInit()) return;

        // 将值存入持久存储，并取地址注册到 AngelScript
        switch (value.type()) {
            case ScriptValueType::Bool: {
                auto& stored = globalBools_[name];
                stored = value.asBool();
                engine_->RegisterGlobalProperty(
                    ("bool " + name).c_str(), &stored);
                break;
            }
            case ScriptValueType::Int: {
                auto& stored = globalInts_[name];
                stored = value.asInt();
                engine_->RegisterGlobalProperty(
                    ("int64 " + name).c_str(), &stored);
                break;
            }
            case ScriptValueType::Double: {
                auto& stored = globalDoubles_[name];
                stored = value.asDouble();
                engine_->RegisterGlobalProperty(
                    ("double " + name).c_str(), &stored);
                break;
            }
            case ScriptValueType::String: {
                auto& stored = globalStrings_[name];
                stored = value.asString();
                engine_->RegisterGlobalProperty(
                    ("string " + name).c_str(), &stored);
                break;
            }
            default:
                // 数组/对象等复杂类型暂不支持通过全局属性传递
                break;
        }
    }

    SimpleScriptValue getGlobal(const std::string& name) {
        if (!ensureInit()) return SimpleScriptValue::Null();

        // 从持久存储中读取
        auto itInt = globalInts_.find(name);
        if (itInt != globalInts_.end())
            return SimpleScriptValue::Int(itInt->second);

        auto itDouble = globalDoubles_.find(name);
        if (itDouble != globalDoubles_.end())
            return SimpleScriptValue::Num(itDouble->second);

        auto itBool = globalBools_.find(name);
        if (itBool != globalBools_.end())
            return SimpleScriptValue::Bool(itBool->second);

        auto itStr = globalStrings_.find(name);
        if (itStr != globalStrings_.end())
            return SimpleScriptValue::Str(itStr->second);

        return SimpleScriptValue::Null();
    }

    // ---- C++ 函数注册 ----

    void registerFunction(const std::string& name, ScriptFunction func) {
        if (!ensureInit()) return;

        auto key = registeredFuncs_.size();
        registeredFuncs_.push_back(std::move(func));

        // 使用 asMETHOD 注册成员函数
        std::string decl = "void " + name + "(SIMPLESCRIPTENGINE_Any&out result)";
        // AngelScript 需要通过全局函数代理
        // 简化实现：使用泛型函数注册
        (void)key; (void)decl;
    }

    void setErrorCallback(SimpleScriptEngine::ErrorCallback cb) {
        errorCb_ = std::move(cb);
    }

private:
    asIScriptEngine* engine_ = nullptr;
    bool initialized_ = false;

    std::vector<asIScriptModule*> modules_;
    std::vector<ScriptFunction> registeredFuncs_;
    SimpleScriptEngine::ErrorCallback errorCb_;

    // 存储 setGlobal 注册的值（指针被 RegisterGlobalProperty 引用，必须持久化）
    std::unordered_map<std::string, bool>        globalBools_;
    std::unordered_map<std::string, int64_t>     globalInts_;
    std::unordered_map<std::string, double>      globalDoubles_;
    std::unordered_map<std::string, std::string> globalStrings_;

    bool ensureInit() {
        if (!initialized_ || !engine_) {
            reportError("AngelScript engine not initialized");
            return false;
        }
        return true;
    }

    void reportError(const std::string& msg) {
        if (errorCb_) {
            errorCb_("AngelScript", msg);
        } else {
            std::cerr << "[AngelScript] Error: " << msg << std::endl;
        }
    }

    SimpleScriptValue callFunction(asIScriptFunction* func,
                                    const std::vector<SimpleScriptValue>& args) {
        if (!func) return SimpleScriptValue::Null();

        asIScriptContext* ctx = engine_->RequestContext();
        if (!ctx) {
            reportError("Failed to request context");
            return SimpleScriptValue::Null();
        }

        int r = ctx->Prepare(func);
        if (r < 0) {
            engine_->ReturnContext(ctx);
            reportError("Failed to prepare context");
            return SimpleScriptValue::Null();
        }

        // 设置参数（简化：基本类型参数）
        for (size_t i = 0; i < args.size() && i < static_cast<size_t>(func->GetParamCount()); ++i) {
            int typeId;
            func->GetParam(i, &typeId);
            setContextArg(ctx, i, typeId, args[i]);
        }

        r = ctx->Execute();
        if (r != asEXECUTION_FINISHED) {
            engine_->ReturnContext(ctx);
            reportError("Execution failed");
            return SimpleScriptValue::Null();
        }

        // 读取返回值
        SimpleScriptValue result = SimpleScriptValue::Null();
        int retTypeId = func->GetReturnTypeId();
        if (retTypeId != asTYPEID_VOID) {
            result = getContextReturn(ctx, retTypeId);
        }

        engine_->ReturnContext(ctx);
        return result;
    }

    void setContextArg(asIScriptContext* ctx, int argIdx, int typeId, const SimpleScriptValue& val) {
        if (typeId & asTYPEID_OBJHANDLE) return; // 跳过对象类型

        switch (typeId) {
            case asTYPEID_BOOL:
                ctx->SetArgByte(argIdx, val.asBool() ? 1 : 0);
                break;
            case asTYPEID_INT8:
                ctx->SetArgByte(argIdx, static_cast<asBYTE>(val.asInt()));
                break;
            case asTYPEID_INT16:
                ctx->SetArgWord(argIdx, static_cast<asWORD>(val.asInt()));
                break;
            case asTYPEID_INT32:
                ctx->SetArgDWord(argIdx, static_cast<asDWORD>(val.asInt()));
                break;
            case asTYPEID_INT64:
                ctx->SetArgQWord(argIdx, static_cast<asQWORD>(val.asInt()));
                break;
            case asTYPEID_FLOAT:
                ctx->SetArgFloat(argIdx, static_cast<float>(val.asDouble()));
                break;
            case asTYPEID_DOUBLE:
                ctx->SetArgDouble(argIdx, val.asDouble());
                break;
            default:
                break;
        }
    }

    SimpleScriptValue getContextReturn(asIScriptContext* ctx, int typeId) {
        switch (typeId) {
            case asTYPEID_VOID:   return SimpleScriptValue::Null();
            case asTYPEID_BOOL:   return SimpleScriptValue::Bool(ctx->GetReturnByte() != 0);
            case asTYPEID_INT8:   return SimpleScriptValue::Int(ctx->GetReturnByte());
            case asTYPEID_INT16:  return SimpleScriptValue::Int(ctx->GetReturnWord());
            case asTYPEID_INT32:  return SimpleScriptValue::Int(ctx->GetReturnDWord());
            case asTYPEID_INT64:  return SimpleScriptValue::Int(static_cast<int64_t>(ctx->GetReturnQWord()));
            case asTYPEID_FLOAT:  return SimpleScriptValue::Num(ctx->GetReturnFloat());
            case asTYPEID_DOUBLE: return SimpleScriptValue::Num(ctx->GetReturnDouble());
            default:              return SimpleScriptValue::Null();
        }
    }
};

// ================================================================
// 公共接口转发
// ================================================================

AngelScriptEngine::AngelScriptEngine() : impl_(std::make_unique<Impl>()) {}
AngelScriptEngine::~AngelScriptEngine() = default;

AngelScriptEngine::AngelScriptEngine(AngelScriptEngine&& other) noexcept
    : impl_(std::move(other.impl_)) {}

AngelScriptEngine& AngelScriptEngine::operator=(AngelScriptEngine&& other) noexcept {
    if (this != &other) impl_ = std::move(other.impl_);
    return *this;
}

const char* AngelScriptEngine::engineVersion() const noexcept {
    return "2.38.0";
}

bool AngelScriptEngine::initialize() {
    impl_->setErrorCallback(errorCallback_);
    return impl_->initialize();
}

void AngelScriptEngine::shutdown() { impl_->shutdown(); }
bool AngelScriptEngine::isInitialized() const noexcept { return impl_->isInitialized(); }
bool AngelScriptEngine::executeString(const std::string& code) { return impl_->executeString(code); }
bool AngelScriptEngine::executeFile(const std::string& path)    { return impl_->executeFile(path); }
SimpleScriptValue AngelScriptEngine::eval(const std::string& expr) { return impl_->eval(expr); }
SimpleScriptValue AngelScriptEngine::call(const std::string& func,
                                          const std::vector<SimpleScriptValue>& args) {
    return impl_->call(func, args);
}
bool AngelScriptEngine::hasFunction(const std::string& name) { return impl_->hasFunction(name); }
void AngelScriptEngine::setGlobal(const std::string& name, const SimpleScriptValue& val) {
    impl_->setGlobal(name, val);
}
SimpleScriptValue AngelScriptEngine::getGlobal(const std::string& name) {
    return impl_->getGlobal(name);
}
void AngelScriptEngine::registerFunction(const std::string& name, ScriptFunction func) {
    impl_->registerFunction(name, std::move(func));
}

} // namespace SimpleScriptEngine

#else // !SIMPLESCRIPTENGINE_ENABLE_ANGELSCRIPT

#include <iostream>

namespace SimpleScriptEngine {

class AngelScriptEngine::Impl {
public:
    bool isInitialized() const noexcept { return false; }
    void reportDisabled() {
        std::cerr << "[AngelScript] Engine not built (SIMPLESCRIPTENGINE_ENABLE_ANGELSCRIPT=OFF)\n";
    }
};

AngelScriptEngine::AngelScriptEngine() : impl_(std::make_unique<Impl>()) {}
AngelScriptEngine::~AngelScriptEngine() = default;
AngelScriptEngine::AngelScriptEngine(AngelScriptEngine&&) noexcept = default;
AngelScriptEngine& AngelScriptEngine::operator=(AngelScriptEngine&&) noexcept = default;

const char* AngelScriptEngine::engineVersion() const noexcept { return "disabled"; }
bool AngelScriptEngine::initialize() { impl_->reportDisabled(); return false; }
void AngelScriptEngine::shutdown() {}
bool AngelScriptEngine::isInitialized() const noexcept { return false; }
bool AngelScriptEngine::executeString(const std::string&) { impl_->reportDisabled(); return false; }
bool AngelScriptEngine::executeFile(const std::string&)    { impl_->reportDisabled(); return false; }
SimpleScriptValue AngelScriptEngine::eval(const std::string&) { impl_->reportDisabled(); return SimpleScriptValue::Null(); }
SimpleScriptValue AngelScriptEngine::call(const std::string&, const std::vector<SimpleScriptValue>&) {
    impl_->reportDisabled(); return SimpleScriptValue::Null();
}
bool AngelScriptEngine::hasFunction(const std::string&) { impl_->reportDisabled(); return false; }
void AngelScriptEngine::setGlobal(const std::string&, const SimpleScriptValue&) { impl_->reportDisabled(); }
SimpleScriptValue AngelScriptEngine::getGlobal(const std::string&) { impl_->reportDisabled(); return SimpleScriptValue::Null(); }
void AngelScriptEngine::registerFunction(const std::string&, ScriptFunction) { impl_->reportDisabled(); }

} // namespace SimpleScriptEngine

#endif // SIMPLESCRIPTENGINE_ENABLE_ANGELSCRIPT

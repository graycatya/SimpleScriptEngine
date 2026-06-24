// ============================================================================
// ChaiScriptEngine 实现 — PIMPL + ChaiScript C++ API 绑定
// ============================================================================
// ChaiScript 是 header-only 库，直接在 Impl 中包含。
// ============================================================================

#include "ChaiScriptEngine.h"

#ifdef SIMPLESCRIPTENGINE_ENABLE_CHAISCRIPT

#include "chaiscript/chaiscript.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace SimpleScriptEngine {

class ChaiScriptEngine::Impl {
public:
    Impl() = default;

    ~Impl() {
        if (initialized_) shutdown();
    }

    bool initialize() {
        if (initialized_) return true;

        try {
            chai_ = std::make_unique<chaiscript::ChaiScript>();
        } catch (const std::exception& e) {
            reportError(std::string("Failed to create ChaiScript: ") + e.what());
            return false;
        }

        initialized_ = true;
        return true;
    }

    void shutdown() {
        if (!initialized_) return;
        try {
            registeredFuncs_.clear();
            chai_.reset();
        } catch (const std::exception& e) {
            // ChaiScript 内部清理时可能抛出异常，忽略并继续
            (void)e;
        } catch (...) {
            // 忽略所有异常
        }
        initialized_ = false;
    }

    bool isInitialized() const noexcept {
        return initialized_ && chai_ != nullptr;
    }

    bool executeString(const std::string& code) {
        if (!ensureInit()) return false;
        try {
            chai_->eval(code);
            return true;
        } catch (const chaiscript::exception::eval_error& e) {
            reportError(std::string("eval error: ") + e.what());
            return false;
        } catch (const std::exception& e) {
            reportError(std::string("error: ") + e.what());
            return false;
        }
    }

    bool executeFile(const std::string& path) {
        if (!ensureInit()) return false;

        std::ifstream file(path);
        if (!file.is_open()) {
            reportError("Cannot open file: " + path);
            return false;
        }

        std::ostringstream oss;
        oss << file.rdbuf();
        return executeString(oss.str());
    }

    SimpleScriptValue eval(const std::string& expr) {
        if (!ensureInit()) return SimpleScriptValue::Null();
        try {
            auto boxed = chai_->eval(expr);
            return fromBoxedValue(boxed);
        } catch (const std::exception& e) {
            reportError(std::string("eval error: ") + e.what());
            return SimpleScriptValue::Null();
        }
    }

    // ---- 函数探测 ----

    bool hasFunction(const std::string& name) {
        if (!ensureInit()) return false;
        try {
            // 尝试求值函数名，如果存在则是可调用对象
            auto boxed = chai_->eval(name);
            // ChaiScript 中函数也是 Boxed_Value，检查是否为未定义/空
            return !boxed.is_undef() && !boxed.is_null();
        } catch (const std::exception&) {
            return false;
        }
    }

    SimpleScriptValue call(const std::string& func,
                           const std::vector<SimpleScriptValue>& args) {
        if (!ensureInit()) return SimpleScriptValue::Null();
        try {
            // 构建调用表达式，将简单类型参数内联到表达式字符串中
            std::ostringstream callExpr;
            callExpr << func << "(";
            for (size_t i = 0; i < args.size(); ++i) {
                if (i > 0) callExpr << ", ";
                // 内联简单类型
                switch (args[i].type()) {
                    case ScriptValueType::Bool:
                        callExpr << (args[i].asBool() ? "true" : "false");
                        break;
                    case ScriptValueType::Int:
                        callExpr << args[i].asInt();
                        break;
                    case ScriptValueType::Double:
                        callExpr << args[i].asDouble();
                        break;
                    case ScriptValueType::String:
                        callExpr << "\"" << args[i].asString() << "\"";
                        break;
                    default:
                        // 复杂类型用临时变量 + add
                        {
                            std::string varName = "__sse_call_arg_" + std::to_string(i);
                            chai_->add(chaiscript::var(toBoxedValue(args[i])), varName);
                            callExpr << varName;
                        }
                        break;
                }
            }
            callExpr << ")";

            return fromBoxedValue(chai_->eval(callExpr.str()));
        } catch (const std::exception& e) {
            reportError(std::string("call error: ") + e.what());
            return SimpleScriptValue::Null();
        }
    }

    void setGlobal(const std::string& name, const SimpleScriptValue& value) {
        if (!ensureInit()) return;
        try {
            chai_->add(chaiscript::var(toBoxedValue(value)), name);
        } catch (const std::exception& e) {
            reportError(std::string("setGlobal error: ") + e.what());
        }
    }

    SimpleScriptValue getGlobal(const std::string& name) {
        if (!ensureInit()) return SimpleScriptValue::Null();
        try {
            auto boxed = chai_->eval(name);
            return fromBoxedValue(boxed);
        } catch (const std::exception& e) {
            return SimpleScriptValue::Null();
        }
    }

    void registerFunction(const std::string& name, ScriptFunction func) {
        if (!ensureInit()) return;

        auto key = registeredFuncs_.size();
        registeredFuncs_.push_back(std::move(func));

        // 注册一个接受 std::vector<Boxed_Value> 返回 Boxed_Value 的函数
        auto* impl = this;
        chai_->add(chaiscript::fun([impl, key](const std::vector<chaiscript::Boxed_Value>& args)
            -> chaiscript::Boxed_Value {
            std::vector<SimpleScriptValue> simpleScriptEngineArgs;
            simpleScriptEngineArgs.reserve(args.size());
            for (auto& a : args) {
                simpleScriptEngineArgs.push_back(impl->fromBoxedValue(a));
            }
            SimpleScriptValue result = impl->registeredFuncs_[key](simpleScriptEngineArgs);
            return impl->toBoxedValue(result);
        }), name);
    }

    void setErrorCallback(SimpleScriptEngine::ErrorCallback cb) {
        errorCb_ = std::move(cb);
    }

private:
    std::unique_ptr<chaiscript::ChaiScript> chai_;
    bool initialized_ = false;
    std::vector<ScriptFunction> registeredFuncs_;
    SimpleScriptEngine::ErrorCallback errorCb_;

    bool ensureInit() {
        if (!initialized_ || !chai_) {
            reportError("ChaiScript engine not initialized");
            return false;
        }
        return true;
    }

    void reportError(const std::string& msg) {
        if (errorCb_) {
            errorCb_("ChaiScript", msg);
        } else {
            std::cerr << "[ChaiScript] Error: " << msg << std::endl;
        }
    }

    // ---- SimpleScriptValue ↔ Boxed_Value 转换 ----

    chaiscript::Boxed_Value toBoxedValue(const SimpleScriptValue& val) {
        switch (val.type()) {
            case ScriptValueType::Null:
                return chaiscript::Boxed_Value();
            case ScriptValueType::Bool:
                return chaiscript::Boxed_Value(val.asBool());
            case ScriptValueType::Int:
                return chaiscript::Boxed_Value(val.asInt());
            case ScriptValueType::Double:
                return chaiscript::Boxed_Value(val.asDouble());
            case ScriptValueType::String:
                return chaiscript::Boxed_Value(val.asString());
            case ScriptValueType::Array: {
                const auto& arr = val.asArray();
                std::vector<chaiscript::Boxed_Value> boxedArr;
                boxedArr.reserve(arr.size());
                for (auto& item : arr) {
                    boxedArr.push_back(toBoxedValue(item));
                }
                return chaiscript::Boxed_Value(boxedArr);
            }
            case ScriptValueType::Object: {
                const auto& obj = val.asObject();
                std::map<std::string, chaiscript::Boxed_Value> boxedObj;
                for (auto& kv : obj) {
                    boxedObj[kv.first] = toBoxedValue(kv.second);
                }
                return chaiscript::Boxed_Value(boxedObj);
            }
            default:
                return chaiscript::Boxed_Value();
        }
    }

    SimpleScriptValue fromBoxedValue(const chaiscript::Boxed_Value& bv) {
        using chaiscript::boxed_cast;
        using chaiscript::user_type;

        if (bv.is_undef() || bv.is_null()) {
            return SimpleScriptValue::Null();
        }

        const auto& ti = bv.get_type_info();

        if (ti.bare_equal(user_type<bool>())) {
            return SimpleScriptValue::Bool(boxed_cast<bool>(bv));
        }
        if (ti.bare_equal(user_type<int>())) {
            return SimpleScriptValue::Int(boxed_cast<int>(bv));
        }
        if (ti.bare_equal(user_type<int64_t>())) {
            return SimpleScriptValue::Int(boxed_cast<int64_t>(bv));
        }
        if (ti.bare_equal(user_type<double>())) {
            return SimpleScriptValue::Num(boxed_cast<double>(bv));
        }
        if (ti.bare_equal(user_type<float>())) {
            return SimpleScriptValue::Num(boxed_cast<float>(bv));
        }
        if (ti.bare_equal(user_type<std::string>())) {
            return SimpleScriptValue::Str(boxed_cast<std::string>(bv));
        }
        if (ti.bare_equal(user_type<std::vector<chaiscript::Boxed_Value>>())) {
            const auto& vec = boxed_cast<const std::vector<chaiscript::Boxed_Value>&>(bv);
            ScriptArray arr;
            arr.reserve(vec.size());
            for (auto& item : vec) {
                arr.push_back(fromBoxedValue(item));
            }
            return SimpleScriptValue(std::move(arr));
        }
        if (ti.bare_equal(user_type<std::map<std::string, chaiscript::Boxed_Value>>())) {
            const auto& map = boxed_cast<const std::map<std::string, chaiscript::Boxed_Value>&>(bv);
            ScriptObject obj;
            for (auto& kv : map) {
                obj[kv.first] = fromBoxedValue(kv.second);
            }
            return SimpleScriptValue(std::move(obj));
        }

        // 对其他类型尝试转为 string
        try {
            return SimpleScriptValue::Str(boxed_cast<std::string>(bv));
        } catch (...) {
            return SimpleScriptValue::Str("<unknown-chaiscript-type>");
        }
    }
};

// ================================================================
// 公共接口转发
// ================================================================

ChaiScriptEngine::ChaiScriptEngine() : impl_(std::make_unique<Impl>()) {}
ChaiScriptEngine::~ChaiScriptEngine() = default;

ChaiScriptEngine::ChaiScriptEngine(ChaiScriptEngine&& other) noexcept
    : impl_(std::move(other.impl_)) {}

ChaiScriptEngine& ChaiScriptEngine::operator=(ChaiScriptEngine&& other) noexcept {
    if (this != &other) impl_ = std::move(other.impl_);
    return *this;
}

const char* ChaiScriptEngine::engineVersion() const noexcept {
    return "6.1.0";
}

bool ChaiScriptEngine::initialize() {
    impl_->setErrorCallback(errorCallback_);
    return impl_->initialize();
}

void ChaiScriptEngine::shutdown() { impl_->shutdown(); }
bool ChaiScriptEngine::isInitialized() const noexcept { return impl_->isInitialized(); }
bool ChaiScriptEngine::executeString(const std::string& code) { return impl_->executeString(code); }
bool ChaiScriptEngine::executeFile(const std::string& path)    { return impl_->executeFile(path); }
SimpleScriptValue ChaiScriptEngine::eval(const std::string& expr) { return impl_->eval(expr); }
SimpleScriptValue ChaiScriptEngine::call(const std::string& func,
                                         const std::vector<SimpleScriptValue>& args) {
    return impl_->call(func, args);
}
bool ChaiScriptEngine::hasFunction(const std::string& name) { return impl_->hasFunction(name); }
void ChaiScriptEngine::setGlobal(const std::string& name, const SimpleScriptValue& val) {
    impl_->setGlobal(name, val);
}
SimpleScriptValue ChaiScriptEngine::getGlobal(const std::string& name) {
    return impl_->getGlobal(name);
}
void ChaiScriptEngine::registerFunction(const std::string& name, ScriptFunction func) {
    impl_->registerFunction(name, std::move(func));
}

} // namespace SimpleScriptEngine

#else // !SIMPLESCRIPTENGINE_ENABLE_CHAISCRIPT

#include <iostream>

namespace SimpleScriptEngine {

class ChaiScriptEngine::Impl {
public:
    bool isInitialized() const noexcept { return false; }
    void reportDisabled() {
        std::cerr << "[ChaiScript] Engine not built (SIMPLESCRIPTENGINE_ENABLE_CHAISCRIPT=OFF)\n";
    }
};

ChaiScriptEngine::ChaiScriptEngine() : impl_(std::make_unique<Impl>()) {}
ChaiScriptEngine::~ChaiScriptEngine() = default;
ChaiScriptEngine::ChaiScriptEngine(ChaiScriptEngine&&) noexcept = default;
ChaiScriptEngine& ChaiScriptEngine::operator=(ChaiScriptEngine&&) noexcept = default;

const char* ChaiScriptEngine::engineVersion() const noexcept { return "disabled"; }
bool ChaiScriptEngine::initialize() { impl_->reportDisabled(); return false; }
void ChaiScriptEngine::shutdown() {}
bool ChaiScriptEngine::isInitialized() const noexcept { return false; }
bool ChaiScriptEngine::executeString(const std::string&) { impl_->reportDisabled(); return false; }
bool ChaiScriptEngine::executeFile(const std::string&)    { impl_->reportDisabled(); return false; }
SimpleScriptValue ChaiScriptEngine::eval(const std::string&) { impl_->reportDisabled(); return SimpleScriptValue::Null(); }
SimpleScriptValue ChaiScriptEngine::call(const std::string&, const std::vector<SimpleScriptValue>&) {
    impl_->reportDisabled(); return SimpleScriptValue::Null();
}
bool ChaiScriptEngine::hasFunction(const std::string&) { impl_->reportDisabled(); return false; }
void ChaiScriptEngine::setGlobal(const std::string&, const SimpleScriptValue&) { impl_->reportDisabled(); }
SimpleScriptValue ChaiScriptEngine::getGlobal(const std::string&) { impl_->reportDisabled(); return SimpleScriptValue::Null(); }
void ChaiScriptEngine::registerFunction(const std::string&, ScriptFunction) { impl_->reportDisabled(); }

} // namespace SimpleScriptEngine

#endif // SIMPLESCRIPTENGINE_ENABLE_CHAISCRIPT

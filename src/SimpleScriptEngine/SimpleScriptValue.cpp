// ============================================================================
// SimpleScriptValue PIMPL implementation
// ============================================================================

#include "SimpleScriptValue.h"

#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace SimpleScriptEngine {

// ================================================================
// Private implementation
// ================================================================

class SimpleScriptValue::Impl {
public:
    ScriptValueType type = ScriptValueType::Null;

    bool      boolVal   = false;
    int64_t   intVal    = 0;
    double    doubleVal = 0.0;

    std::string                   strVal;
    std::shared_ptr<ScriptArray>  arrayVal;
    std::shared_ptr<ScriptObject> objectVal;
    std::shared_ptr<ScriptFunction> funcVal;

    void copyFrom(const Impl& other) {
        type      = other.type;
        boolVal   = other.boolVal;
        intVal    = other.intVal;
        doubleVal = other.doubleVal;
        strVal    = other.strVal;
        arrayVal  = other.arrayVal;
        objectVal = other.objectVal;
        funcVal   = other.funcVal;
    }

    void destroy() {
        arrayVal.reset();
        objectVal.reset();
        funcVal.reset();
        strVal.clear();
    }
};

// ================================================================
// Lifecycle
// ================================================================

SimpleScriptValue::SimpleScriptValue()
    : impl_(std::make_unique<Impl>()) {}

SimpleScriptValue::~SimpleScriptValue() = default;

SimpleScriptValue::SimpleScriptValue(const SimpleScriptValue& other)
    : impl_(std::make_unique<Impl>()) {
    impl_->copyFrom(*other.impl_);
}

SimpleScriptValue& SimpleScriptValue::operator=(const SimpleScriptValue& other) {
    if (this != &other) {
        impl_->destroy();
        impl_->copyFrom(*other.impl_);
    }
    return *this;
}

SimpleScriptValue::SimpleScriptValue(SimpleScriptValue&& other) noexcept
    : impl_(std::move(other.impl_)) {
    other.impl_ = std::make_unique<Impl>();
}

SimpleScriptValue& SimpleScriptValue::operator=(SimpleScriptValue&& other) noexcept {
    if (this != &other) {
        impl_ = std::move(other.impl_);
        other.impl_ = std::make_unique<Impl>();
    }
    return *this;
}

// ================================================================
// Type queries
// ================================================================

ScriptValueType SimpleScriptValue::type() const noexcept { return impl_->type; }

bool SimpleScriptValue::isNull()     const noexcept { return impl_->type == ScriptValueType::Null; }
bool SimpleScriptValue::isBool()     const noexcept { return impl_->type == ScriptValueType::Bool; }
bool SimpleScriptValue::isInt()      const noexcept { return impl_->type == ScriptValueType::Int; }
bool SimpleScriptValue::isDouble()   const noexcept { return impl_->type == ScriptValueType::Double; }
bool SimpleScriptValue::isNumber()   const noexcept { return isInt() || isDouble(); }
bool SimpleScriptValue::isString()   const noexcept { return impl_->type == ScriptValueType::String; }
bool SimpleScriptValue::isArray()    const noexcept { return impl_->type == ScriptValueType::Array; }
bool SimpleScriptValue::isObject()   const noexcept { return impl_->type == ScriptValueType::Object; }
bool SimpleScriptValue::isFunction() const noexcept { return impl_->type == ScriptValueType::Function; }

// ================================================================
// Value accessors
// ================================================================

bool SimpleScriptValue::asBool() const {
    switch (impl_->type) {
        case ScriptValueType::Null:   return false;
        case ScriptValueType::Bool:   return impl_->boolVal;
        case ScriptValueType::Int:    return impl_->intVal != 0;
        case ScriptValueType::Double: return impl_->doubleVal != 0.0;
        case ScriptValueType::String: return !impl_->strVal.empty();
        case ScriptValueType::Array:  return impl_->arrayVal && !impl_->arrayVal->empty();
        case ScriptValueType::Object: return impl_->objectVal && !impl_->objectVal->empty();
        case ScriptValueType::Function:return true;
        default: return false;
    }
}

int64_t SimpleScriptValue::asInt() const {
    switch (impl_->type) {
        case ScriptValueType::Null:   return 0;
        case ScriptValueType::Bool:   return impl_->boolVal ? 1 : 0;
        case ScriptValueType::Int:    return impl_->intVal;
        case ScriptValueType::Double: return static_cast<int64_t>(impl_->doubleVal);
        case ScriptValueType::String: return std::stoll(impl_->strVal);
        default: return 0;
    }
}

double SimpleScriptValue::asDouble() const {
    switch (impl_->type) {
        case ScriptValueType::Null:   return 0.0;
        case ScriptValueType::Bool:   return impl_->boolVal ? 1.0 : 0.0;
        case ScriptValueType::Int:    return static_cast<double>(impl_->intVal);
        case ScriptValueType::Double: return impl_->doubleVal;
        case ScriptValueType::String: return std::stod(impl_->strVal);
        default: return 0.0;
    }
}

float SimpleScriptValue::asFloat() const {
    return static_cast<float>(asDouble());
}

const std::string& SimpleScriptValue::asString() const {
    if (impl_->type == ScriptValueType::String) {
        return impl_->strVal;
    }
    static thread_local std::string s_empty;
    s_empty = toString();
    return s_empty;
}

const ScriptArray& SimpleScriptValue::asArray() const {
    static const ScriptArray s_empty;
    return (impl_->type == ScriptValueType::Array && impl_->arrayVal) ? *impl_->arrayVal : s_empty;
}

const ScriptObject& SimpleScriptValue::asObject() const {
    static const ScriptObject s_empty;
    return (impl_->type == ScriptValueType::Object && impl_->objectVal) ? *impl_->objectVal : s_empty;
}

const ScriptFunction& SimpleScriptValue::asFunction() const {
    static const ScriptFunction s_empty = [](const std::vector<SimpleScriptValue>&) {
        return SimpleScriptValue::null();
    };
    return (impl_->type == ScriptValueType::Function && impl_->funcVal) ? *impl_->funcVal : s_empty;
}

// ================================================================
// Implicit conversions
// ================================================================

SimpleScriptValue::operator bool()        const { return asBool(); }
SimpleScriptValue::operator int64_t()     const { return asInt(); }
SimpleScriptValue::operator double()      const { return asDouble(); }
SimpleScriptValue::operator std::string() const { return asString(); }

// ================================================================
// Factories
// ================================================================

SimpleScriptValue SimpleScriptValue::null() {
    SimpleScriptValue v;
    return v;
}

SimpleScriptValue SimpleScriptValue::boolean(bool val) {
    SimpleScriptValue v;
    v.impl_->type    = ScriptValueType::Bool;
    v.impl_->boolVal = val;
    return v;
}

SimpleScriptValue SimpleScriptValue::integer(int64_t val) {
    SimpleScriptValue v;
    v.impl_->type   = ScriptValueType::Int;
    v.impl_->intVal = val;
    return v;
}

SimpleScriptValue SimpleScriptValue::number(double val) {
    SimpleScriptValue v;
    v.impl_->type      = ScriptValueType::Double;
    v.impl_->doubleVal = val;
    return v;
}

SimpleScriptValue SimpleScriptValue::string(const char* val) {
    SimpleScriptValue v;
    v.impl_->type   = ScriptValueType::String;
    v.impl_->strVal = val ? val : "";
    return v;
}

SimpleScriptValue SimpleScriptValue::string(const std::string& val) {
    SimpleScriptValue v;
    v.impl_->type   = ScriptValueType::String;
    v.impl_->strVal = val;
    return v;
}

SimpleScriptValue SimpleScriptValue::array(ScriptArray val) {
    SimpleScriptValue v;
    v.impl_->type      = ScriptValueType::Array;
    v.impl_->arrayVal  = std::make_shared<ScriptArray>(std::move(val));
    return v;
}

SimpleScriptValue SimpleScriptValue::object(ScriptObject val) {
    SimpleScriptValue v;
    v.impl_->type       = ScriptValueType::Object;
    v.impl_->objectVal  = std::make_shared<ScriptObject>(std::move(val));
    return v;
}

SimpleScriptValue SimpleScriptValue::function(ScriptFunction val) {
    SimpleScriptValue v;
    v.impl_->type     = ScriptValueType::Function;
    v.impl_->funcVal  = std::make_shared<ScriptFunction>(std::move(val));
    return v;
}

SimpleScriptValue SimpleScriptValue::arr(std::initializer_list<SimpleScriptValue> items) {
    return array(ScriptArray(items.begin(), items.end()));
}

SimpleScriptValue SimpleScriptValue::obj(std::initializer_list<std::pair<const char*, SimpleScriptValue>> pairs) {
    ScriptObject obj;
    for (auto& p : pairs) {
        obj.emplace(p.first, p.second);
    }
    return object(std::move(obj));
}

// ================================================================
// Utilities
// ================================================================

std::string SimpleScriptValue::toString() const {
    switch (impl_->type) {
        case ScriptValueType::Null:   return "null";
        case ScriptValueType::Bool:   return impl_->boolVal ? "true" : "false";
        case ScriptValueType::Int:    return std::to_string(impl_->intVal);
        case ScriptValueType::Double: return std::to_string(impl_->doubleVal);
        case ScriptValueType::String: return "\"" + impl_->strVal + "\"";
        case ScriptValueType::Array: {
            std::ostringstream oss;
            oss << "[";
            if (impl_->arrayVal) {
                for (size_t i = 0; i < impl_->arrayVal->size(); ++i) {
                    if (i > 0) oss << ", ";
                    oss << (*impl_->arrayVal)[i].toString();
                }
            }
            oss << "]";
            return oss.str();
        }
        case ScriptValueType::Object: {
            std::ostringstream oss;
            oss << "{";
            if (impl_->objectVal) {
                bool first = true;
                for (auto& kv : *impl_->objectVal) {
                    if (!first) oss << ", ";
                    oss << kv.first << ": " << kv.second.toString();
                    first = false;
                }
            }
            oss << "}";
            return oss.str();
        }
        case ScriptValueType::Function: return "<function>";
        default: return "<unknown>";
    }
}

const char* SimpleScriptValue::typeName() const {
    switch (impl_->type) {
        case ScriptValueType::Null:     return "Null";
        case ScriptValueType::Bool:     return "Bool";
        case ScriptValueType::Int:      return "Int";
        case ScriptValueType::Double:   return "Double";
        case ScriptValueType::String:   return "String";
        case ScriptValueType::Array:    return "Array";
        case ScriptValueType::Object:   return "Object";
        case ScriptValueType::Function: return "Function";
        default: return "Unknown";
    }
}

} // namespace SimpleScriptEngine

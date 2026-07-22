#pragma once
// ============================================================================
// SimpleScriptValue — PIMPL-based cross-engine value type (ABI-stable)
// ============================================================================
// sizeof() is fixed across versions. Internal storage hidden in .cpp.
// All methods are out-of-line — no inline accessors that leak layout.
// ============================================================================

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace SimpleScriptEngine {

enum class ScriptValueType {
    Null,
    Bool,
    Int,
    Double,
    String,
    Array,
    Object,
    Function
};

class SimpleScriptValue;

// Type aliases (stable — backed by opaque pointers internally)
using ScriptFunction = std::function<SimpleScriptValue(const std::vector<SimpleScriptValue>&)>;
using ScriptObject  = std::unordered_map<std::string, SimpleScriptValue>;
using ScriptArray   = std::vector<SimpleScriptValue>;

class SimpleScriptValue {
public:
    // ---- lifecycle ----
    SimpleScriptValue();
    ~SimpleScriptValue();

    SimpleScriptValue(const SimpleScriptValue& other);
    SimpleScriptValue& operator=(const SimpleScriptValue& other);
    SimpleScriptValue(SimpleScriptValue&& other) noexcept;
    SimpleScriptValue& operator=(SimpleScriptValue&& other) noexcept;

    // ---- type queries (out-of-line, ABI-stable) ----
    ScriptValueType type() const noexcept;
    bool isNull()     const noexcept;
    bool isBool()     const noexcept;
    bool isInt()      const noexcept;
    bool isDouble()   const noexcept;
    bool isNumber()   const noexcept;
    bool isString()   const noexcept;
    bool isArray()    const noexcept;
    bool isObject()   const noexcept;
    bool isFunction() const noexcept;

    // ---- accessors ----
    bool        asBool()     const;
    int64_t     asInt()      const;
    double      asDouble()   const;
    float       asFloat()    const;
    const std::string&        asString()   const;
    const ScriptArray&        asArray()    const;
    const ScriptObject&       asObject()   const;
    const ScriptFunction&     asFunction() const;

    // ---- implicit conversions ----
    explicit operator bool()        const;
    explicit operator int64_t()     const;
    explicit operator double()      const;
    explicit operator std::string() const;

    // ---- factories (only these create SimpleScriptValue) ----
    static SimpleScriptValue null();
    static SimpleScriptValue boolean(bool v);
    static SimpleScriptValue integer(int64_t v);
    static SimpleScriptValue number(double v);
    static SimpleScriptValue string(const char* v);
    static SimpleScriptValue string(const std::string& v);
    static SimpleScriptValue array(ScriptArray v);
    static SimpleScriptValue object(ScriptObject v);
    static SimpleScriptValue function(ScriptFunction v);
    static SimpleScriptValue arr(std::initializer_list<SimpleScriptValue> items);
    static SimpleScriptValue obj(std::initializer_list<std::pair<const char*, SimpleScriptValue>> pairs);

    // ---- utilities ----
    std::string toString() const;
    const char* typeName() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace SimpleScriptEngine

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace SimpleScriptEngine {

// ============================================================================
// SimpleScriptValue — 跨引擎统一值类型
// ============================================================================
// 用于 C++ ↔ 脚本之间传递数据，屏蔽各引擎内部类型差异。
// 设计为值语义（copy/move），轻量级，适合频繁传参。
// ============================================================================

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

/// 脚本可调用的 C++ 函数签名
using ScriptFunction = std::function<SimpleScriptValue(const std::vector<SimpleScriptValue>&)>;

/// 对象类型（用于 Object 值）
using ScriptObject = std::unordered_map<std::string, SimpleScriptValue>;

/// 数组类型
using ScriptArray = std::vector<SimpleScriptValue>;

// ============================================================================
// SimpleScriptValue 实现
// ============================================================================

class SimpleScriptValue {
public:
    // ---- 构造 / 析构 ----

    SimpleScriptValue() noexcept;
    explicit SimpleScriptValue(bool val) noexcept;
    explicit SimpleScriptValue(int32_t val) noexcept;
    explicit SimpleScriptValue(int64_t val) noexcept;
    explicit SimpleScriptValue(uint32_t val) noexcept;
    explicit SimpleScriptValue(uint64_t val) noexcept;
    explicit SimpleScriptValue(double val) noexcept;
    explicit SimpleScriptValue(float val) noexcept;
    SimpleScriptValue(const char* val);
    SimpleScriptValue(const std::string& val);
    explicit SimpleScriptValue(ScriptArray val);
    explicit SimpleScriptValue(ScriptObject val);
    explicit SimpleScriptValue(ScriptFunction val);

    ~SimpleScriptValue();

    // 拷贝 / 移动
    SimpleScriptValue(const SimpleScriptValue& other);
    SimpleScriptValue& operator=(const SimpleScriptValue& other);
    SimpleScriptValue(SimpleScriptValue&& other) noexcept;
    SimpleScriptValue& operator=(SimpleScriptValue&& other) noexcept;

    // ---- 类型查询 ----

    ScriptValueType type() const noexcept { return type_; }

    bool isNull()    const noexcept { return type_ == ScriptValueType::Null; }
    bool isBool()    const noexcept { return type_ == ScriptValueType::Bool; }
    bool isInt()     const noexcept { return type_ == ScriptValueType::Int; }
    bool isDouble()  const noexcept { return type_ == ScriptValueType::Double; }
    bool isNumber()  const noexcept { return isInt() || isDouble(); }
    bool isString()  const noexcept { return type_ == ScriptValueType::String; }
    bool isArray()   const noexcept { return type_ == ScriptValueType::Array; }
    bool isObject()  const noexcept { return type_ == ScriptValueType::Object; }
    bool isFunction()const noexcept { return type_ == ScriptValueType::Function; }

    // ---- 值访问器 ----

    bool        asBool()     const;
    int64_t     asInt()      const;
    double      asDouble()   const;
    float       asFloat()    const { return static_cast<float>(asDouble()); }
    const std::string&  asString()  const;
    const ScriptArray&  asArray()   const;
    const ScriptObject& asObject()  const;
    const ScriptFunction& asFunction() const;

    // 隐式转换支持
    explicit operator bool()        const { return asBool(); }
    explicit operator int64_t()     const { return asInt(); }
    explicit operator double()      const { return asDouble(); }
    explicit operator std::string() const { return asString(); }

    // ---- 工厂方法 ----

    static SimpleScriptValue Null() { return SimpleScriptValue(); }
    static SimpleScriptValue Bool(bool v) { return SimpleScriptValue(v); }
    static SimpleScriptValue Int(int64_t v) { return SimpleScriptValue(v); }
    static SimpleScriptValue Num(double v) { return SimpleScriptValue(v); }
    static SimpleScriptValue Str(const std::string& v) { return SimpleScriptValue(v); }
    static SimpleScriptValue Arr(std::initializer_list<SimpleScriptValue> items);
    static SimpleScriptValue Obj(std::initializer_list<std::pair<const char*, SimpleScriptValue>> pairs);

    // ---- 便捷方法 ----

    /// 返回易于调试的字符串表示
    std::string toString() const;

    /// 类型名称
    const char* typeName() const;

private:
    ScriptValueType type_ = ScriptValueType::Null;

    // 标量存储（避免小对象堆分配）
    bool     boolVal_   = false;
    int64_t  intVal_    = 0;
    double   doubleVal_ = 0.0;

    // 复杂类型用 shared_ptr 共享
    std::string                        strVal_;
    std::shared_ptr<ScriptArray>       arrayVal_;
    std::shared_ptr<ScriptObject>      objectVal_;
    std::shared_ptr<ScriptFunction>    funcVal_;

    void copyFrom(const SimpleScriptValue& other);
    void destory();
};

} // namespace SimpleScriptEngine

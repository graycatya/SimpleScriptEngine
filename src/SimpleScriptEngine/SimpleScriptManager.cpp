#include "SimpleScriptManager.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <sstream>

namespace SimpleScriptEngine {

// ============================================================================
// 文件扩展名 → 引擎名称 映射表
// ============================================================================

static const std::unordered_map<std::string, std::string> kDefaultExtensionMap = {
    {".lua",  "LuaJIT"},
    {".js",   "QuickJS"},
    {".chai", "ChaiScript"},
    {".as",   "AngelScript"},
};

// ============================================================================
// SimpleScriptValue 实现
// ============================================================================

SimpleScriptValue::SimpleScriptValue() noexcept
    : type_(ScriptValueType::Null) {}

SimpleScriptValue::SimpleScriptValue(bool val) noexcept
    : type_(ScriptValueType::Bool), boolVal_(val) {}

SimpleScriptValue::SimpleScriptValue(int32_t val) noexcept
    : type_(ScriptValueType::Int), intVal_(static_cast<int64_t>(val)) {}

SimpleScriptValue::SimpleScriptValue(int64_t val) noexcept
    : type_(ScriptValueType::Int), intVal_(val) {}

SimpleScriptValue::SimpleScriptValue(uint32_t val) noexcept
    : type_(ScriptValueType::Int), intVal_(static_cast<int64_t>(val)) {}

SimpleScriptValue::SimpleScriptValue(uint64_t val) noexcept
    : type_(ScriptValueType::Int), intVal_(static_cast<int64_t>(val)) {}

SimpleScriptValue::SimpleScriptValue(double val) noexcept
    : type_(ScriptValueType::Double), doubleVal_(val) {}

SimpleScriptValue::SimpleScriptValue(float val) noexcept
    : type_(ScriptValueType::Double), doubleVal_(static_cast<double>(val)) {}

SimpleScriptValue::SimpleScriptValue(const char* val)
    : type_(ScriptValueType::String), strVal_(val ? val : "") {}

SimpleScriptValue::SimpleScriptValue(const std::string& val)
    : type_(ScriptValueType::String), strVal_(val) {}

SimpleScriptValue::SimpleScriptValue(ScriptArray val)
    : type_(ScriptValueType::Array)
    , arrayVal_(std::make_shared<ScriptArray>(std::move(val))) {}

SimpleScriptValue::SimpleScriptValue(ScriptObject val)
    : type_(ScriptValueType::Object)
    , objectVal_(std::make_shared<ScriptObject>(std::move(val))) {}

SimpleScriptValue::SimpleScriptValue(ScriptFunction val)
    : type_(ScriptValueType::Function)
    , funcVal_(std::make_shared<ScriptFunction>(std::move(val))) {}

SimpleScriptValue::~SimpleScriptValue() {
    destory();
}

void SimpleScriptValue::copyFrom(const SimpleScriptValue& other) {
    type_      = other.type_;
    boolVal_   = other.boolVal_;
    intVal_    = other.intVal_;
    doubleVal_ = other.doubleVal_;
    strVal_    = other.strVal_;
    arrayVal_  = other.arrayVal_;
    objectVal_ = other.objectVal_;
    funcVal_   = other.funcVal_;
}

void SimpleScriptValue::destory() {
    // shared_ptr 自动管理生命周期
    arrayVal_.reset();
    objectVal_.reset();
    funcVal_.reset();
}

SimpleScriptValue::SimpleScriptValue(const SimpleScriptValue& other) {
    copyFrom(other);
}

SimpleScriptValue& SimpleScriptValue::operator=(const SimpleScriptValue& other) {
    if (this != &other) {
        destory();
        copyFrom(other);
    }
    return *this;
}

SimpleScriptValue::SimpleScriptValue(SimpleScriptValue&& other) noexcept
    : type_(other.type_)
    , boolVal_(other.boolVal_)
    , intVal_(other.intVal_)
    , doubleVal_(other.doubleVal_)
    , strVal_(std::move(other.strVal_))
    , arrayVal_(std::move(other.arrayVal_))
    , objectVal_(std::move(other.objectVal_))
    , funcVal_(std::move(other.funcVal_))
{
    other.type_ = ScriptValueType::Null;
}

SimpleScriptValue& SimpleScriptValue::operator=(SimpleScriptValue&& other) noexcept {
    if (this != &other) {
        destory();
        type_      = other.type_;
        boolVal_   = other.boolVal_;
        intVal_    = other.intVal_;
        doubleVal_ = other.doubleVal_;
        strVal_    = std::move(other.strVal_);
        arrayVal_  = std::move(other.arrayVal_);
        objectVal_ = std::move(other.objectVal_);
        funcVal_   = std::move(other.funcVal_);
        other.type_ = ScriptValueType::Null;
    }
    return *this;
}

// ---- 值访问器 ----

bool SimpleScriptValue::asBool() const {
    switch (type_) {
        case ScriptValueType::Null:   return false;
        case ScriptValueType::Bool:   return boolVal_;
        case ScriptValueType::Int:    return intVal_ != 0;
        case ScriptValueType::Double: return doubleVal_ != 0.0;
        case ScriptValueType::String: return !strVal_.empty();
        case ScriptValueType::Array:  return arrayVal_ && !arrayVal_->empty();
        case ScriptValueType::Object: return objectVal_ && !objectVal_->empty();
        case ScriptValueType::Function:return true;
        default: return false;
    }
}

int64_t SimpleScriptValue::asInt() const {
    switch (type_) {
        case ScriptValueType::Null:   return 0;
        case ScriptValueType::Bool:   return boolVal_ ? 1 : 0;
        case ScriptValueType::Int:    return intVal_;
        case ScriptValueType::Double: return static_cast<int64_t>(doubleVal_);
        case ScriptValueType::String: return std::stoll(strVal_);
        default: return 0;
    }
}

double SimpleScriptValue::asDouble() const {
    switch (type_) {
        case ScriptValueType::Null:   return 0.0;
        case ScriptValueType::Bool:   return boolVal_ ? 1.0 : 0.0;
        case ScriptValueType::Int:    return static_cast<double>(intVal_);
        case ScriptValueType::Double: return doubleVal_;
        case ScriptValueType::String: return std::stod(strVal_);
        default: return 0.0;
    }
}

const std::string& SimpleScriptValue::asString() const {
    if (type_ == ScriptValueType::String) {
        return strVal_;
    }
    // 返回静态缓存字符串（非线程安全的便捷实现）
    static thread_local std::string s_temp;
    s_temp = toString();
    return s_temp;
}

const ScriptArray& SimpleScriptValue::asArray() const {
    static const ScriptArray s_empty;
    return (type_ == ScriptValueType::Array && arrayVal_) ? *arrayVal_ : s_empty;
}

const ScriptObject& SimpleScriptValue::asObject() const {
    static const ScriptObject s_empty;
    return (type_ == ScriptValueType::Object && objectVal_) ? *objectVal_ : s_empty;
}

const ScriptFunction& SimpleScriptValue::asFunction() const {
    static const ScriptFunction s_empty = [](const std::vector<SimpleScriptValue>&) {
        return SimpleScriptValue::Null();
    };
    return (type_ == ScriptValueType::Function && funcVal_) ? *funcVal_ : s_empty;
}

// ---- 工厂方法 ----

SimpleScriptValue SimpleScriptValue::Arr(std::initializer_list<SimpleScriptValue> items) {
    return SimpleScriptValue(ScriptArray(items.begin(), items.end()));
}

SimpleScriptValue SimpleScriptValue::Obj(std::initializer_list<std::pair<const char*, SimpleScriptValue>> pairs) {
    ScriptObject obj;
    for (auto& p : pairs) {
        obj.emplace(p.first, p.second);
    }
    return SimpleScriptValue(std::move(obj));
}

// ---- 调试 ----

std::string SimpleScriptValue::toString() const {
    switch (type_) {
        case ScriptValueType::Null:   return "null";
        case ScriptValueType::Bool:   return boolVal_ ? "true" : "false";
        case ScriptValueType::Int:    return std::to_string(intVal_);
        case ScriptValueType::Double: return std::to_string(doubleVal_);
        case ScriptValueType::String: return "\"" + strVal_ + "\"";
        case ScriptValueType::Array: {
            std::ostringstream oss;
            oss << "[";
            if (arrayVal_) {
                for (size_t i = 0; i < arrayVal_->size(); ++i) {
                    if (i > 0) oss << ", ";
                    oss << (*arrayVal_)[i].toString();
                }
            }
            oss << "]";
            return oss.str();
        }
        case ScriptValueType::Object: {
            std::ostringstream oss;
            oss << "{";
            if (objectVal_) {
                bool first = true;
                for (auto& kv : *objectVal_) {
                    if (!first) oss << ", ";
                    oss << kv.first << ": " << kv.second.toString();
                    first = false;
                }
            }
            oss << "}";
            return oss.str();
        }
        case ScriptValueType::Function:
            return "<function>";
        default:
            return "<unknown>";
    }
}

const char* SimpleScriptValue::typeName() const {
    switch (type_) {
        case ScriptValueType::Null:    return "Null";
        case ScriptValueType::Bool:    return "Bool";
        case ScriptValueType::Int:     return "Int";
        case ScriptValueType::Double:  return "Double";
        case ScriptValueType::String:  return "String";
        case ScriptValueType::Array:   return "Array";
        case ScriptValueType::Object:  return "Object";
        case ScriptValueType::Function:return "Function";
        default: return "Unknown";
    }
}

// ============================================================================
// SimpleScriptManager 实现
// ============================================================================

SimpleScriptManager& SimpleScriptManager::instance() {
    static SimpleScriptManager s_instance;
    return s_instance;
}

SimpleScriptManager::~SimpleScriptManager() {
    shutdownAll();
}

bool SimpleScriptManager::registerEngine(std::unique_ptr<SimpleScriptEngine> engine) {
    if (!engine) return false;

    std::lock_guard<std::mutex> lock(mutex_);
    const std::string name(engine->engineName());

    if (engines_.count(name)) {
        std::cerr << "[SimpleScriptManager] Engine '" << name << "' already registered, skipping.\n";
        return false;
    }

    const std::string ext(engine->fileExtension());
    engines_.emplace(name, std::move(engine));

    // 建立扩展名→引擎名称映射
    if (!ext.empty()) {
        extensionMap_[ext] = name;
    }

    return true;
}

bool SimpleScriptManager::unregisterEngine(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = engines_.find(name);
    if (it == engines_.end()) return false;

    // 关闭引擎
    if (it->second->isInitialized()) {
        it->second->shutdown();
    }

    // 清理扩展名映射
    for (auto eit = extensionMap_.begin(); eit != extensionMap_.end(); ) {
        if (eit->second == name) {
            eit = extensionMap_.erase(eit);
        } else {
            ++eit;
        }
    }

    engines_.erase(it);
    return true;
}

SimpleScriptEngine* SimpleScriptManager::getEngine(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = engines_.find(name);
    return (it != engines_.end()) ? it->second.get() : nullptr;
}

std::vector<std::string> SimpleScriptManager::listEngines() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    names.reserve(engines_.size());
    for (auto& kv : engines_) {
        names.push_back(kv.first);
    }
    return names;
}

size_t SimpleScriptManager::initializeAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = 0;
    for (auto& kv : engines_) {
        if (!kv.second->isInitialized()) {
            if (kv.second->initialize()) {
                ++count;
            }
        } else {
            ++count; // 已初始化也算
        }
    }
    return count;
}

void SimpleScriptManager::shutdownAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& kv : engines_) {
        if (kv.second->isInitialized()) {
            kv.second->shutdown();
        }
    }
}

bool SimpleScriptManager::executeString(const std::string& engineName, const std::string& code) {
    auto* engine = getEngine(engineName);
    if (!engine) {
        std::cerr << "[SimpleScriptManager] Engine '" << engineName << "' not found.\n";
        return false;
    }
    if (!engine->isInitialized()) {
        if (!engine->initialize()) {
            std::cerr << "[SimpleScriptManager] Failed to initialize engine '" << engineName << "'.\n";
            return false;
        }
    }
    return engine->executeString(code);
}

bool SimpleScriptManager::executeFile(const std::string& path) {
    // 从路径提取扩展名
    auto dotPos = path.rfind('.');
    if (dotPos == std::string::npos) {
        std::cerr << "[SimpleScriptManager] No file extension found in '" << path << "'.\n";
        return false;
    }

    std::string ext = path.substr(dotPos);
    // 转小写
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    auto* engine = findEngineByExtension(ext);
    if (!engine) {
        std::cerr << "[SimpleScriptManager] No engine registered for extension '" << ext << "'.\n";
        return false;
    }

    if (!engine->isInitialized()) {
        if (!engine->initialize()) {
            std::cerr << "[SimpleScriptManager] Failed to initialize engine '"
                      << engine->engineName() << "'.\n";
            return false;
        }
    }

    return engine->executeFile(path);
}

bool SimpleScriptManager::hasFunction(const std::string& engineName, const std::string& funcName) {
    auto* engine = getEngine(engineName);
    if (!engine) {
        std::cerr << "[SimpleScriptManager] Engine '" << engineName << "' not found.\n";
        return false;
    }
    if (!engine->isInitialized()) {
        if (!engine->initialize()) return false;
    }
    return engine->hasFunction(funcName);
}

SimpleScriptEngine* SimpleScriptManager::findEngineByExtension(const std::string& extension) const {
    std::lock_guard<std::mutex> lock(mutex_);

    // 先在运行时注册的映射中查找
    auto it = extensionMap_.find(extension);
    if (it != extensionMap_.end()) {
        auto eit = engines_.find(it->second);
        if (eit != engines_.end()) return eit->second.get();
    }

    // 回退到默认映射表
    auto dit = kDefaultExtensionMap.find(extension);
    if (dit != kDefaultExtensionMap.end()) {
        auto eit = engines_.find(dit->second);
        if (eit != engines_.end()) return eit->second.get();
    }

    return nullptr;
}

void SimpleScriptManager::setSharedGlobal(const std::string& name, const SimpleScriptValue& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    sharedGlobals_[name] = value;

    // 同步写入所有已初始化的引擎
    for (auto& kv : engines_) {
        if (kv.second->isInitialized()) {
            kv.second->setGlobal(name, value);
        }
    }
}

SimpleScriptValue SimpleScriptManager::getSharedGlobal(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sharedGlobals_.find(name);
    return (it != sharedGlobals_.end()) ? it->second : SimpleScriptValue::Null();
}

bool SimpleScriptManager::hasSharedGlobal(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sharedGlobals_.count(name) > 0;
}

void SimpleScriptManager::updateExtensionMap() {
    extensionMap_.clear();
    for (auto& kv : engines_) {
        const std::string ext(kv.second->fileExtension());
        if (!ext.empty()) {
            extensionMap_[ext] = kv.first;
        }
    }
}

} // namespace SimpleScriptEngine

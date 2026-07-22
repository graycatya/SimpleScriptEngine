#include "SimpleScriptManager.h"

#ifdef SIMPLESCRIPTENGINE_ENABLE_PAK_PROTECTION
    #include "Crypto/SSEPakLoader.h"
#endif

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
    return (it != sharedGlobals_.end()) ? it->second : SimpleScriptValue::null();
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

// ================================================================
// Encrypted package execution (hidden implementation)
// ================================================================

bool SimpleScriptManager::executePak(const std::string& pakPath) {
#ifdef SIMPLESCRIPTENGINE_ENABLE_PAK_PROTECTION
    Crypto::SSEPakLoader loader;
    Crypto::LoadResult result = loader.loadPak(pakPath, *this);
    return result.successFiles > 0;
#else
    (void)pakPath;
    return false;
#endif
}

} // namespace SimpleScriptEngine

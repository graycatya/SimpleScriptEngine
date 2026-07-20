// ============================================================================
// SSEPakLoader.cpp — SimpleScriptEngine 集成加载器实现
// ============================================================================

#include "SSEPakLoader.h"
#include "SSECrypto.h"

#include <algorithm>
#include <cctype>
#include <iostream>

namespace SimpleScriptEngine {
namespace Crypto {

// ================================================================
// 引擎名映射
// ================================================================

EngineType SSEPakLoader::engineNameToType(const std::string& name) {
    if (name == "LuaJIT")      return EngineType::LuaJIT;
    if (name == "QuickJS")     return EngineType::QuickJS;
    if (name == "ChaiScript")  return EngineType::ChaiScript;
    if (name == "AngelScript") return EngineType::AngelScript;
    return EngineType::Unknown;
}

EngineType SSEPakLoader::extensionToType(const std::string& ext) {
    if (ext == ".lua")  return EngineType::LuaJIT;
    if (ext == ".js")   return EngineType::QuickJS;
    if (ext == ".chai") return EngineType::ChaiScript;
    if (ext == ".as")   return EngineType::AngelScript;
    return EngineType::Unknown;
}

// ================================================================
// 构造/析构
// ================================================================

SSEPakLoader::SSEPakLoader() = default;
SSEPakLoader::~SSEPakLoader() {
    close();
}

// ================================================================
// openPak / close
// ================================================================

bool SSEPakLoader::openPak(const std::string& pakPath) {
    return reader_.open(pakPath);
}

void SSEPakLoader::close() {
    reader_.close();
}

// ================================================================
// loadPak - 加载到单个引擎
// ================================================================

LoadResult SSEPakLoader::loadPak(const std::string& pakPath,
                                  SimpleScriptEngine& engine)
{
    LoadResult result;

    if (!reader_.open(pakPath)) {
        result.failures.push_back({pakPath, "Failed to open .ssep file"});
        return result;
    }

    // 生成错误回调名称
    std::string engineName(engine.engineName());

    auto files = reader_.listFiles();
    result.totalFiles = static_cast<uint32_t>(files.size());

    for (const auto& fileName : files) {
        auto buf = reader_.getFile(fileName);
        if (buf.empty()) {
            result.failedFiles++;
            result.failures.push_back({fileName, "Failed to extract from pak"});
            continue;
        }

        bool ok = engine.executeString(
            std::string(reinterpret_cast<const char*>(buf.ptr()), buf.size()));
        // SecureBuffer 析构时自动擦除

        if (ok) {
            result.successFiles++;
        } else {
            result.failedFiles++;
            result.failures.push_back({fileName,
                "Execution failed in engine '" + engineName + "'"});
        }
    }

    reader_.close();
    return result;
}

// ================================================================
// loadPak - 加载到 SimpleScriptManager (按引擎路由)
// ================================================================

LoadResult SSEPakLoader::loadPak(const std::string& pakPath,
                                  SimpleScriptManager& manager)
{
    LoadResult result;

    if (!reader_.open(pakPath)) {
        result.failures.push_back({pakPath, "Failed to open .ssep file"});
        return result;
    }

    auto files = reader_.listFiles();
    result.totalFiles = static_cast<uint32_t>(files.size());

    for (const auto& fileName : files) {
        auto buf = reader_.getFile(fileName);
        if (buf.empty()) {
            result.failedFiles++;
            result.failures.push_back({fileName, "Failed to extract from pak"});
            continue;
        }

        // 从文件名提取扩展名，找到对应引擎
        auto dotPos = fileName.rfind('.');
        std::string engineName;

        if (dotPos != std::string::npos) {
            std::string ext = fileName.substr(dotPos);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            EngineType etype = extensionToType(ext);
            switch (etype) {
                case EngineType::LuaJIT:      engineName = "LuaJIT";      break;
                case EngineType::QuickJS:     engineName = "QuickJS";     break;
                case EngineType::ChaiScript:  engineName = "ChaiScript";  break;
                case EngineType::AngelScript: engineName = "AngelScript"; break;
                default: engineName = "";
            }
        }

        if (engineName.empty()) {
            result.failedFiles++;
            result.failures.push_back({fileName, "Unknown engine type for extension"});
            continue;
        }

        std::string code(reinterpret_cast<const char*>(buf.ptr()), buf.size());
        bool ok = manager.executeString(engineName, code);
        // SecureBuffer 析构时自动擦除

        if (ok) {
            result.successFiles++;
        } else {
            result.failedFiles++;
            result.failures.push_back({fileName,
                "Execution failed in engine '" + engineName + "'"});
        }
    }

    reader_.close();
    return result;
}

// ================================================================
// executeBuffer - 底层执行 (按引擎类型)
// ================================================================

bool SSEPakLoader::executeBuffer(SimpleScriptEngine& engine,
                                  EngineType /*entryType*/,
                                  const uint8_t* data, size_t size)
{
    if (!engine.isInitialized()) {
        if (!engine.initialize()) return false;
    }

    // 转换为字符串执行
    // 如果引擎支持 loadBuffer (如 LuaJIT 的 luaL_loadbuffer)，
    // 可以直接使用。当前统一走 executeString
    std::string code(reinterpret_cast<const char*>(data), size);

    bool result = engine.executeString(code);

    // 执行完成后，源码已经在引擎内部，外部可以安全清除
    // 注: 引擎内部可能仍保留源码/字节码引用，取决于引擎实现

    return result;
}

} // namespace Crypto
} // namespace SimpleScriptEngine

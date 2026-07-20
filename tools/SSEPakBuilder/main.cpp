// ============================================================================
// SSEPakBuilder — .ssep 包打包命令行工具
// ============================================================================
//
// 用法:
//   SSEPakBuilder -o output.ssep [options] file1.lua file2.js ...
//
// 选项:
//   -o <path>      输出 .ssep 文件路径 (必填)
//   -k <keyid>     密钥槽位 (0-255, 默认 0)
//   -c             启用 Zlib 压缩
//   -e             启用 AES-256-GCM 加密
//   -h             显示帮助
//
// 文件命名约定:
//   文件名扩展名决定引擎类型:
//     .lua  → LuaJIT, .js → QuickJS, .chai → ChaiScript, .as → AngelScript
//
// 示例:
//   SSEPakBuilder -o scripts.ssep -c -e main.lua config.lua logic.js
//   生成加密+压缩的 scripts.ssep，包含 3 个脚本文件
//
// ============================================================================

// Crypto 头文件 (通过 CMake target_include_directories 引入)
#include "SSEPakFormat.h"
#include "SSECrypto.h"
#include "SSEKeyManager.h"

#include <openssl/rand.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace SimpleScriptEngine::Crypto;

// ================================================================
// 命令行参数
// ================================================================

struct BuildOptions {
    std::string outputPath;
    std::vector<std::string> inputFiles;
    uint32_t keyID     = 0;
    bool compress      = false;
    bool encrypt       = true;
    bool verbose       = false;
    std::string customKeyHex;   // user-provided master key (hex string)
};

// ================================================================
// 文件信息
// ================================================================

struct InputFile {
    std::string path;
    std::string name;       // 包内文件名 (仅文件名部分)
    EngineType engineType;
    std::vector<uint8_t> data;
    std::vector<uint8_t> compressed;  // 压缩后
    bool isCompressed;
};

// ================================================================
// 工具函数
// ================================================================

static EngineType extensionToType(const std::string& path) {
    auto dot = path.rfind('.');
    if (dot == std::string::npos) return EngineType::Unknown;

    std::string ext = path.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".lua")  return EngineType::LuaJIT;
    if (ext == ".js")   return EngineType::QuickJS;
    if (ext == ".chai") return EngineType::ChaiScript;
    if (ext == ".as")   return EngineType::AngelScript;
    return EngineType::Unknown;
}

static std::string extractFileName(const std::string& path) {
    auto slash = path.find_last_of("/\\");
    return (slash != std::string::npos) ? path.substr(slash + 1) : path;
}

static uint64_t xxhash64(const uint8_t* data, size_t len, uint64_t seed = 0) {
    const uint64_t P1 = 0x9E3779B185EBCA87ULL;
    const uint64_t P2 = 0xC2B2AE3D27D4EB4FULL;
    const uint64_t P3 = 0x165667B19E3779F9ULL;
    const uint64_t P4 = 0x85EBCA77C2B2AE63ULL;
    const uint64_t P5 = 0x27D4EB2F165667C5ULL;

    uint64_t h64 = seed + P5 + len;
    size_t i = 0;

    for (; i + 8 <= len; i += 8) {
        uint64_t k;
        memcpy(&k, data + i, 8);
        k *= P2; k = (k << 31) | (k >> 33); k *= P1;
        h64 ^= k;
        h64 = ((h64 << 27) | (h64 >> 37)) * P1 + P4;
    }

    if (i < len) {
        uint64_t tail = 0;
        memcpy(&tail, data + i, len - i);
        tail *= P5; tail = (tail << 31) | (tail >> 33); tail *= P1;
        h64 ^= tail;
    }

    h64 ^= len;
    h64 ^= h64 >> 33; h64 *= P2;
    h64 ^= h64 >> 29; h64 *= P3;
    h64 ^= h64 >> 32;
    return h64;
}



// ================================================================
// 构建 .ssep 文件
// ================================================================

static bool buildSSEP(const BuildOptions& opts) {
    // 1. 读取所有输入文件
    std::vector<InputFile> files;
    uint64_t totalOrigSize = 0;

    for (const auto& path : opts.inputFiles) {
        InputFile f;
        f.path       = path;
        f.name       = extractFileName(path);
        f.engineType = extensionToType(path);

        if (f.engineType == EngineType::Unknown) {
            std::cerr << "[SSEPakBuilder] Warning: Unknown engine type for '"
                      << path << "', skipping\n";
            continue;
        }

        // 读取文件
        std::ifstream ifs(path, std::ios::binary | std::ios::ate);
        if (!ifs) {
            std::cerr << "[SSEPakBuilder] Cannot open: " << path << "\n";
            return false;
        }

        size_t size = static_cast<size_t>(ifs.tellg());
        ifs.seekg(0);
        f.data.resize(size);
        ifs.read(reinterpret_cast<char*>(f.data.data()), size);

        if (opts.verbose) {
            std::cout << "  + " << f.name << " (" << size << " bytes, "
                      << engineTypeName(f.engineType) << ")\n";
        }

        totalOrigSize += size;
        files.push_back(std::move(f));
    }

    if (files.empty()) {
        std::cerr << "[SSEPakBuilder] No input files\n";
        return false;
    }

    // 2. 生成 Header
    SSEPakHeader header = {};
    header.magic        = SSEP_MAGIC;
    header.version      = SSEP_VERSION;
    header.flags        = 0;
    header.pakUUID      = KeyManager::generateUUID();
    header.keyID        = opts.keyID;
    header.fileCount    = static_cast<uint32_t>(files.size());
    header.originalSize = totalOrigSize;

    if (opts.compress) header.flags |= SSEP_FLAG_COMPRESSED;
    if (opts.encrypt)  header.flags |= SSEP_FLAG_ENCRYPTED;
    if (!opts.customKeyHex.empty()) header.flags |= SSEP_FLAG_CUSTOM_KEY;

    // 生成 IV (存于 header 中)
    KeyManager::generateIV(header.iv);

    // 3. 压缩 (可选)
    uint64_t totalCompSize = 0;

    if (opts.compress) {
        for (auto& f : files) {
            std::vector<uint8_t> comp;
            if (compressData(f.data.data(), f.data.size(), comp)) {
                f.compressed = std::move(comp);
                f.isCompressed = true;
            } else {
            }
            totalCompSize += f.compressed.size();
        }
    } else {
        for (auto& f : files) {
            f.compressed = f.data;  // 未压缩
            f.isCompressed = false;
            totalCompSize += f.compressed.size();
        }
    }

    // 4. 构建索引段
    uint64_t currentDataOffset = 0;
    std::vector<uint8_t> indexSection;

    for (auto& f : files) {
        SSEFileEntry entry;
        entry.engineType     = static_cast<uint8_t>(f.engineType);
        entry.nameHash       = xxhash64(
            reinterpret_cast<const uint8_t*>(f.name.c_str()),
            f.name.size(), header.pakUUID);
        entry.nameLen        = static_cast<uint16_t>(f.name.size());
        entry.dataOffset     = currentDataOffset;
        entry.compressedSize = f.compressed.size();
        entry.originalSize   = f.data.size();
        entry.originalCRC    = crc32(f.data.data(), f.data.size());

        // 写入索引
        auto* entryBytes = reinterpret_cast<const uint8_t*>(&entry);
        indexSection.insert(indexSection.end(), entryBytes, entryBytes + sizeof(SSEFileEntry));

        // 写入文件名
        indexSection.insert(indexSection.end(),
                           reinterpret_cast<const uint8_t*>(&f.name[0]),
                           reinterpret_cast<const uint8_t*>(&f.name[0]) + f.name.size());

        currentDataOffset += entry.compressedSize;
    }

    header.indexSize = indexSection.size();
    header.dataSize  = totalCompSize;

    // 5. 构建数据段
    std::vector<uint8_t> dataSection;
    for (auto& f : files) {
        dataSection.insert(dataSection.end(), f.compressed.begin(), f.compressed.end());
    }

    // 6. 计算 HMAC
    uint8_t hmacKey[SSEP_HMAC_SIZE];
    memset(hmacKey, 0x5A, sizeof(hmacKey));  // 简化: 固定 HMAC key
    // 注: 生产环境应使用 HKDF 从主密钥派生 HMAC key

    auto indexHMAC = HMACEngine::compute(indexSection.data(), indexSection.size(),
                                         hmacKey, sizeof(hmacKey));
    auto dataHMAC  = HMACEngine::compute(dataSection.data(), dataSection.size(),
                                         hmacKey, sizeof(hmacKey));

    // 7. 组装加密段明文: [Index] [Data] [Trailer]
    std::vector<uint8_t> plainSection;
    plainSection.insert(plainSection.end(), indexSection.begin(), indexSection.end());
    plainSection.insert(plainSection.end(), dataSection.begin(), dataSection.end());

    SSEPakTrailer trailer = {};
    if (!indexHMAC.empty()) memcpy(trailer.indexHMAC, indexHMAC.data(), SSEP_HMAC_SIZE);
    if (!dataHMAC.empty())  memcpy(trailer.dataHMAC, dataHMAC.data(), SSEP_HMAC_SIZE);

    plainSection.insert(plainSection.end(),
                        reinterpret_cast<uint8_t*>(&trailer),
                        reinterpret_cast<uint8_t*>(&trailer) + sizeof(SSEPakTrailer));

    // 8. 加密 (可选)
    std::vector<uint8_t> finalEncrypted;

    if (opts.encrypt) {
        uint8_t aesKey[SSEP_AES_KEY_SIZE];
        uint8_t aesIV[SSEP_GCM_IV_SIZE];

        if (!KeyManager::instance().deriveKeys(header, aesKey, aesIV)) {
            std::cerr << "[SSEPakBuilder] Key derivation failed\n";
            return false;
        }

        uint8_t gcmTag[SSEP_GCM_TAG_SIZE];
        finalEncrypted = AESCipher::encrypt(plainSection, aesKey, aesIV, gcmTag);

        secureZero(aesKey, sizeof(aesKey));

        if (finalEncrypted.size() != plainSection.size()) {
            std::cerr << "[SSEPakBuilder] Encryption failed: output size mismatch\n";
            return false;
        }

        // 将 GCM tag 追加到密文末尾
        finalEncrypted.insert(finalEncrypted.end(), gcmTag, gcmTag + SSEP_GCM_TAG_SIZE);
    } else {
        // 不加密: 明文 + trailer 直接输出 (开发模式)
        finalEncrypted = plainSection;
    }

    // 9. 计算 Header CRC32
    header.headerCRC = crc32(
        reinterpret_cast<const uint8_t*>(&header),
        offsetof(SSEPakHeader, headerCRC));

    // 10. 写入文件
    std::ofstream out(opts.outputPath, std::ios::binary);
    if (!out) {
        std::cerr << "[SSEPakBuilder] Cannot create output: " << opts.outputPath << "\n";
        return false;
    }

    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    out.write(reinterpret_cast<const char*>(finalEncrypted.data()), finalEncrypted.size());
    out.close();

    // 打印统计
    size_t totalSize = sizeof(header) + finalEncrypted.size();
    std::cout << "\n[SSEPakBuilder] Package created: " << opts.outputPath << "\n";
    std::cout << "  Files:       " << header.fileCount << "\n";
    std::cout << "  Original:    " << totalOrigSize << " bytes\n";
    std::cout << "  Package:     " << totalSize << " bytes\n";
    std::cout << "  Ratio:       " << (totalOrigSize > 0 ?
        (totalSize * 100 / totalOrigSize) : 0) << "%\n";
    std::cout << "  Compressed:  " << (opts.compress ? "yes" : "no") << "\n";
    std::cout << "  Encrypted:   " << (opts.encrypt ? "AES-256-GCM" : "no") << "\n";
    std::cout << "  Pak UUID:    0x" << std::hex << header.pakUUID << std::dec << "\n";

    // 安全清理
    secureZero(hmacKey, sizeof(hmacKey));
    secureZero(plainSection);
    for (auto& f : files) {
        secureZero(f.data);
        secureZero(f.compressed);
    }

    return true;
}

// ================================================================
// 帮助信息
// ================================================================

static void printHelp(const char* prog) {
    std::cout << "SSEPakBuilder v1.0 -- SimpleScriptEngine Protected Package Builder\n\n"
              << "Usage: " << prog << " -o <output.ssep> [options] <files...>\n\n"
              << "Options:\n"
              << "  -o <path>    Output .ssep file path (required)\n"
              << "  -k <keyid>   Key slot ID (0-255, default: 0)\n"
              << "  --key-hex <hex>  Custom master key (hex, min 32 chars)\n"
              << "  -c           Enable Zlib compression\n"
              << "  -e           Enable AES-256-GCM encryption (default: on)\n"
              << "  --noenc      Disable encryption (plaintext, dev mode only)\n"
              << "  -v           Verbose output\n"
              << "  -h           Show this help\n\n"
              << "File type detection by extension:\n"
              << "  .lua  -> LuaJIT\n"
              << "  .js   -> QuickJS\n"
              << "  .chai -> ChaiScript\n"
              << "  .as   -> AngelScript\n\n"
              << "Example:\n"
              << "  " << prog << " -o scripts.ssep -c -e main.lua config.lua logic.js\n";
}

// ================================================================
// Main
// ================================================================

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printHelp(argv[0]);
        return 1;
    }

    BuildOptions opts;

    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);

        if (arg == "-h" || arg == "--help") {
            printHelp(argv[0]);
            return 0;
        } else if (arg == "-o") {
            if (i + 1 < argc) opts.outputPath = argv[++i];
        } else if (arg == "-k") {
            if (i + 1 < argc) opts.keyID = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "-c") {
            opts.compress = true;
        } else if (arg == "-e") {
            opts.encrypt = true;
        } else if (arg == "--noenc") {
            opts.encrypt = false;
        } else if (arg == "--key-hex") {
            if (i + 1 < argc) opts.customKeyHex = argv[++i];
        } else if (arg == "-v") {
            opts.verbose = true;
        } else if (arg[0] != '-') {
            opts.inputFiles.push_back(arg);
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            return 1;
        }
    }

    if (opts.outputPath.empty()) {
        std::cerr << "[SSEPakBuilder] No output file specified (-o)\n";
        return 1;
    }

    if (opts.inputFiles.empty()) {
        std::cerr << "[SSEPakBuilder] No input files specified\n";
        return 1;
    }

    if (opts.verbose) {
        std::cout << "[SSEPakBuilder] Input files: " << opts.inputFiles.size() << "\n";
        std::cout << "[SSEPakBuilder] Output: " << opts.outputPath << "\n";
        std::cout << "[SSEPakBuilder] Compress: " << (opts.compress ? "yes" : "no") << "\n";
        std::cout << "[SSEPakBuilder] Encrypt: " << (opts.encrypt ? "yes" : "no") << "\n";
        if (!opts.customKeyHex.empty()) {
            std::cout << "[SSEPakBuilder] Using custom key\n";
        }
        std::cout << "[SSEPakBuilder] Reading files...\n";
    }

    // 设置自定义密钥
    if (!opts.customKeyHex.empty()) {
        if (!KeyManager::instance().setCustomKeyHex(opts.customKeyHex.c_str())) {
            std::cerr << "[SSEPakBuilder] Invalid custom key hex (need >= 32 hex chars)\n";
            return 1;
        }
    }

    if (!buildSSEP(opts)) {
        return 1;
    }

    return 0;
}

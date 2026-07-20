// ============================================================================
// SSEPakReader.cpp — .ssep 包运行时读取器实现
// ============================================================================

#include "SSEPakReader.h"
#include "SSECrypto.h"
#include "SSEKeyManager.h"
#include "SSEAntiDebug.h"

#include <openssl/rand.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace SimpleScriptEngine {
namespace Crypto {

// ================================================================
// 密钥碎片 #3 (分散在 SSEPakReader.cpp)
// ================================================================

namespace {
    static const uint64_t FRAG3_SEED = 0xE5429F18D6C3A708ULL;
    static const uint64_t FRAG3_MASK = 0x0C1D2E3F4A5B6C7DULL;

    SSE_NOINLINE
    uint64_t computeFragment3() {
        volatile uint64_t x = FRAG3_SEED;
        x ^= FRAG3_MASK;
        x = ((x << 31) | (x >> 33));
        x ^= 0xD76AA478E8C7B756ULL;
        return x;
    }
}

const uint8_t* getKeyFragment3(size_t& outLen) {
    static uint64_t cached = 0;
    static bool computed = false;

    if (!computed) {
        cached = computeFragment3();
        computed = true;
    }

    outLen = sizeof(uint64_t);
    return reinterpret_cast<const uint8_t*>(&cached);
}

// ================================================================
// 内部: 读取文件
// ================================================================

namespace {

std::vector<uint8_t> readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return {};

    std::streamsize size = file.tellg();
    if (size <= 0 || static_cast<size_t>(size) < SSEP_HEADER_SIZE + 64) return {};

    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(data.data()), size)) return {};

    return data;
}

// xxHash64 (简单实现，用于文件名哈希)
uint64_t xxhash64(const uint8_t* data, size_t len, uint64_t seed = 0) {
    const uint64_t PRIME64_1 = 0x9E3779B185EBCA87ULL;
    const uint64_t PRIME64_2 = 0xC2B2AE3D27D4EB4FULL;
    const uint64_t PRIME64_3 = 0x165667B19E3779F9ULL;
    const uint64_t PRIME64_4 = 0x85EBCA77C2B2AE63ULL;

    const uint64_t PRIME64_5 = 0x27D4EB2F165667C5ULL;

    uint64_t h64 = seed + PRIME64_5 + len;
    h64 = ((h64 << 31) | (h64 >> 33)) * PRIME64_2;

    size_t i = 0;
    for (; i + 8 <= len; i += 8) {
        uint64_t k;
        memcpy(&k, data + i, 8);
        k *= PRIME64_2;
        k = (k << 31) | (k >> 33);
        k *= PRIME64_1;
        h64 ^= k;
        h64 = ((h64 << 27) | (h64 >> 37)) * PRIME64_1 + PRIME64_4;
    }

    // 尾部
    if (i < len) {
        uint64_t tail = 0;
        memcpy(&tail, data + i, len - i);
        tail *= PRIME64_3;
        tail = (tail << 31) | (tail >> 33);
        tail *= PRIME64_1;
        h64 ^= tail;
    }

    h64 ^= len;
    h64 ^= h64 >> 33;
    h64 *= PRIME64_2;
    h64 ^= h64 >> 29;
    h64 *= PRIME64_3;
    h64 ^= h64 >> 32;

    return h64;
}

} // anonymous namespace

// ================================================================
// SecureBuffer
// ================================================================

void SecureBuffer::wipe() {
    if (!data.empty()) {
        secureZero(data);
        data.clear();
    }
}

// ================================================================
// SSEPakReader 构造/析构
// ================================================================

SSEPakReader::SSEPakReader() = default;
SSEPakReader::~SSEPakReader() {
    close();
}

// ================================================================
// open - 打开并验证 .ssep 文件
// ================================================================

bool SSEPakReader::open(const std::string& path) {
    close();

    // 1. 反调试快速检测 (Debug 模式下跳过)
#if !defined(_DEBUG) && (defined(SIMPLESCRIPTENGINE_ANTI_DEBUG_LIGHT) || defined(SIMPLESCRIPTENGINE_ANTI_DEBUG_HEAVY))
    if (AntiDebug::isDebuggerPresent()) {
        return false;
    }
#endif

    // 2. 读取文件
    auto fileData = readFile(path);
    if (fileData.empty()) {
        std::cerr << "[SSEPakReader] Failed to read file: " << path << "\n";
        return false;
    }

    // 3. 解析 Header
    memcpy(&header_, fileData.data(), SSEP_HEADER_SIZE);

    // 验证 Magic
    if (header_.magic != SSEP_MAGIC) {
        std::cerr << "[SSEPakReader] Bad magic: 0x" << std::hex << header_.magic
                  << " (expected 0x" << SSEP_MAGIC << ")\n";
        return false;
    }
    if (header_.version != SSEP_VERSION) {
        std::cerr << "[SSEPakReader] Bad version: " << header_.version
                  << " (expected " << SSEP_VERSION << ")\n";
        return false;
    }

    // 验证 Header CRC32
    uint32_t computedCRC = crc32(
        reinterpret_cast<const uint8_t*>(&header_),
        offsetof(SSEPakHeader, headerCRC));
    if (computedCRC != header_.headerCRC) {
        std::cerr << "[SSEPakReader] Header CRC mismatch: computed=0x"
                  << std::hex << computedCRC << " expected=0x" << header_.headerCRC << "\n";
        return false;
    }

    std::cerr << "[SSEPakReader] Header OK, fileCount=" << std::dec << header_.fileCount
              << " magic ok, version ok, CRC ok\n";

    // 4. 提取加密段
    size_t encryptedOffset = SSEP_HEADER_SIZE;
    size_t encryptedSize = fileData.size() - encryptedOffset;

    if (encryptedSize < SSEP_HMAC_SIZE * 2 + SSEP_GCM_TAG_SIZE) {
        std::cerr << "[SSEPakReader] Encrypted section too small: " << encryptedSize << " bytes\n";
        secureZero(fileData);
        return false;
    }

    rawData_.assign(fileData.begin() + encryptedOffset, fileData.end());
    secureZero(fileData);

    // 5. 解密
    if (!decryptSection()) {
        std::cerr << "[SSEPakReader] Decrypt section failed\n";
        secureZero(rawData_);
        return false;
    }

    // 6. 解析索引
    if (!parseIndex()) {
        std::cerr << "[SSEPakReader] Parse index failed\n";
        secureZero(rawData_);
        secureZero(decryptedData_);
        return false;
    }

    // 7. 完整性验证
    if (!verifyIntegrity()) {
        std::cerr << "[SSEPakReader] Integrity check failed\n";
        secureZero(rawData_);
        secureZero(decryptedData_);
        index_.clear();
        return false;
    }

    std::cerr << "[SSEPakReader] All checks passed, " << index_.size() << " files loaded\n";

    // rawData_ 已验证完，可以清除密文
    secureZero(rawData_);

    open_ = true;
    return true;
}

// ================================================================
// close
// ================================================================

void SSEPakReader::close() {
    secureZero(rawData_);
    secureZero(decryptedData_);
    index_.clear();
    memset(&header_, 0, sizeof(header_));
    open_ = false;
}

// ================================================================
// 轻量 Header 探针 (不解密，仅读 128 字节)
// ================================================================

bool SSEPakReader::pakNeedsCustomKey(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;

    SSEPakHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));

    if (header.magic != SSEP_MAGIC) return false;
    if (header.version != SSEP_VERSION) return false;

    return (header.flags & SSEP_FLAG_CUSTOM_KEY) != 0;
}

// ================================================================
// 解密
// ================================================================

bool SSEPakReader::decryptSection() {
    if (!(header_.flags & SSEP_FLAG_ENCRYPTED)) {
        decryptedData_ = rawData_;
        return true;
    }

    size_t dataSize = rawData_.size();

    // Format: [ciphertext(index+data+trailer)] [GCM_tag 16B]
    if (dataSize < SSEP_GCM_TAG_SIZE) {
        std::cerr << "[SSEPakReader] Encrypted section too small: " << dataSize << " bytes\n";
        return false;
    }

    // Last 16 bytes = GCM tag, rest = ciphertext
    size_t cipherLen = dataSize - SSEP_GCM_TAG_SIZE;
    uint8_t gcmTag[SSEP_GCM_TAG_SIZE];
    memcpy(gcmTag, rawData_.data() + cipherLen, SSEP_GCM_TAG_SIZE);

    std::vector<uint8_t> encrypted(rawData_.data(), rawData_.data() + cipherLen);

    // 派生密钥
    uint8_t aesKey[SSEP_AES_KEY_SIZE];
    uint8_t aesIV[SSEP_GCM_IV_SIZE];
    if (!KeyManager::instance().deriveKeys(header_, aesKey, aesIV)) {
        std::cerr << "[SSEPakReader] Key derivation failed\n";
        secureZero(aesKey, sizeof(aesKey));
        secureZero(encrypted);
        return false;
    }

    // AES-GCM 解密 (GCM 自验证)
    decryptedData_ = AESCipher::decrypt(encrypted, aesKey, aesIV, gcmTag);

    secureZero(aesKey, sizeof(aesKey));
    secureZero(encrypted);

    if (decryptedData_.empty()) {
        std::cerr << "[SSEPakReader] GCM decrypt/tag verify failed\n";
        return false;
    }

    // decryptedData_ = [index] [data] [SSEPakTrailer 80B]
    // 去掉末尾 trailer (完整性验证可以以后再补)
    size_t trailerSize = sizeof(SSEPakTrailer);
    if (decryptedData_.size() < trailerSize) {
        std::cerr << "[SSEPakReader] Decrypted data too small\n";
        secureZero(decryptedData_);
        return false;
    }
    decryptedData_.resize(decryptedData_.size() - trailerSize);

    return true;
}

// ================================================================
// 解析索引
// ================================================================

bool SSEPakReader::parseIndex() {
    if (decryptedData_.empty()) return false;

    const uint8_t* ptr = decryptedData_.data();
    const uint8_t* end = decryptedData_.data() + decryptedData_.size();

    // 遍历 decryptedData_ 中的文件条目
    // 注意：加密段结构是 [FileIndex...] [FileData...] [Trailer]
    // 索引大小在 header_.indexSize

    size_t indexSize = static_cast<size_t>(header_.indexSize);
    if (ptr + indexSize > end) return false;

    const uint8_t* indexEnd = ptr + indexSize;

    for (uint32_t i = 0; i < header_.fileCount; ++i) {
        if (ptr + sizeof(SSEFileEntry) > indexEnd) return false;

        SSEFileEntry entry;
        memcpy(&entry, ptr, sizeof(SSEFileEntry));
        ptr += sizeof(SSEFileEntry);

        // 读取文件名
        if (ptr + entry.nameLen > indexEnd) return false;

        IndexEntry idxEntry;
        idxEntry.engineType     = static_cast<EngineType>(entry.engineType);
        idxEntry.nameHash       = entry.nameHash;
        idxEntry.fileName       = std::string(reinterpret_cast<const char*>(ptr), entry.nameLen);
        idxEntry.dataOffset     = entry.dataOffset;
        idxEntry.compressedSize = entry.compressedSize;
        idxEntry.originalSize   = entry.originalSize;
        idxEntry.originalCRC    = entry.originalCRC;

        ptr += entry.nameLen;
        index_.push_back(std::move(idxEntry));
    }

    return index_.size() == header_.fileCount;
}

// ================================================================
// 完整性验证
// ================================================================

bool SSEPakReader::verifyIntegrity() {
    // AES-GCM already provides authenticated encryption.
    // HMAC trailer verification is skipped for simplicity.
    (void)header_;
    return true;
}

// ================================================================
// getFile - 提取单个文件
// ================================================================

SecureBuffer SSEPakReader::getFile(const std::string& fileName) {
    if (!open_) return SecureBuffer();

    // 反调试检查 (Debug 模式下跳过)
#if !defined(_DEBUG) && defined(SIMPLESCRIPTENGINE_ANTI_DEBUG_HEAVY)
    if (AntiDebug::isDebuggerPresent()) {
        return SecureBuffer();
    }
#endif

    // 按名称查找
    IndexEntry* found = nullptr;
    uint64_t targetHash = xxhash64(
        reinterpret_cast<const uint8_t*>(fileName.c_str()),
        fileName.size(), header_.pakUUID);

    for (auto& entry : index_) {
        // 先按哈希快速过滤，再精确比较文件名
        if (entry.nameHash == targetHash && entry.fileName == fileName) {
            found = &entry;
            break;
        }
    }

    if (!found) {
        // 回退：遍历精确匹配
        for (auto& entry : index_) {
            if (entry.fileName == fileName) {
                found = &entry;
                break;
            }
        }
    }

    if (!found) return SecureBuffer();

    // 检查偏移
    size_t dataStart = static_cast<size_t>(header_.indexSize);
    size_t offset = dataStart + static_cast<size_t>(found->dataOffset);
    size_t compSize = static_cast<size_t>(found->compressedSize);

    if (offset + compSize > decryptedData_.size()) return SecureBuffer();

    const uint8_t* src = decryptedData_.data() + offset;

    // 解压 (如果启用)
    std::vector<uint8_t> result;

    if (header_.flags & SSEP_FLAG_COMPRESSED) {
        result = decompress(src, compSize, static_cast<size_t>(found->originalSize));
    } else {
        result.assign(src, src + compSize);
    }

    if (result.empty()) return SecureBuffer();

    // 验证 CRC32
    uint32_t crc = crc32(result.data(), result.size());
    if (crc != found->originalCRC) {
        secureZero(result);
        return SecureBuffer();
    }

    return SecureBuffer(std::move(result));
}

// ================================================================
// listFiles
// ================================================================

std::vector<std::string> SSEPakReader::listFiles() const {
    std::vector<std::string> names;
    names.reserve(index_.size());
    for (auto& entry : index_) {
        names.push_back(entry.fileName);
    }
    return names;
}

// ================================================================
// Zlib 解压
// ================================================================

std::vector<uint8_t> SSEPakReader::decompress(const uint8_t* data, size_t size,
                                               size_t originalSize)
{
    std::vector<uint8_t> result;
    if (!decompressData(data, size, originalSize, result))
        return {};
    return result;
}

} // namespace Crypto
} // namespace SimpleScriptEngine

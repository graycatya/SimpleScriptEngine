#pragma once
// ============================================================================
// SSECrypto.h — AES-256-GCM + HMAC-SHA256 + HKDF 封装 (基于 OpenSSL EVP)
// ============================================================================
// 所有加密操作通过 OpenSSL EVP 高级 API，禁用已废弃的低级 API。
// ============================================================================

#include "SSEPakFormat.h"

#include <cstdint>
#include <string>
#include <vector>

// ================================================================
// 跨平台 noinline 宏
// ================================================================

#if defined(_MSC_VER)
    #define SSE_NOINLINE __declspec(noinline)
#else
    #define SSE_NOINLINE __attribute__((noinline))
#endif

namespace SimpleScriptEngine {
namespace Crypto {

// ================================================================
// AES-256-GCM 加密/解密
// ================================================================

class AESCipher {
public:
    /// AES-256-GCM 加密
    /// @param plaintext  明文数据
    /// @param key        32 字节 AES 密钥
    /// @param iv         12 字节初始化向量
    /// @param tag_out    输出 16 字节 GCM 认证标签
    /// @return 密文 (与明文等长)
    static std::vector<uint8_t> encrypt(
        const std::vector<uint8_t>& plaintext,
        const uint8_t* key,
        const uint8_t* iv,
        uint8_t* tag_out);

    /// AES-256-GCM 解密
    /// @param ciphertext 密文数据
    /// @param key        32 字节 AES 密钥
    /// @param iv         12 字节初始化向量
    /// @param tag        16 字节 GCM 认证标签 (输入验证)
    /// @return 明文 (解密 + 验证通过), 空 vector 表示失败
    static std::vector<uint8_t> decrypt(
        const std::vector<uint8_t>& ciphertext,
        const uint8_t* key,
        const uint8_t* iv,
        const uint8_t* tag);
};

// ================================================================
// HMAC-SHA256
// ================================================================

class HMACEngine {
public:
    static std::vector<uint8_t> compute(
        const uint8_t* data,
        size_t dataLen,
        const uint8_t* key,
        size_t keyLen);

    static bool verify(
        const uint8_t* data,
        size_t dataLen,
        const uint8_t* key,
        size_t keyLen,
        const uint8_t* expectedHMAC);
};

// ================================================================
// HKDF-SHA256 密钥派生 (RFC 5869)
// ================================================================

class HKDF {
public:
    /// 从主密钥派生出 AES 密钥
    /// @param masterKey  主密钥材料
    /// @param salt       salt (pakUUID 等)
    /// @param info       上下文信息 ("SSEP-v1")
    /// @param keyOut     输出 32 字节 AES 密钥
    /// @param ivOut      输出 12 字节 GCM IV
    /// @return true 成功
    static bool deriveKeyAndIV(
        const std::vector<uint8_t>& masterKey,
        const uint8_t* salt, size_t saltLen,
        const char* info,
        uint8_t* keyOut,
        uint8_t* ivOut);
};

// ================================================================
// CRC32 (用于头部校验)
// ================================================================

uint32_t crc32(const uint8_t* data, size_t len);

// ================================================================
// 安全内存擦除 (防止编译器优化掉)
// ================================================================

void secureZero(void* ptr, size_t len);

template<typename T>
void secureZero(T& container) {
    if (!container.empty()) {
        secureZero(container.data(), container.size());
    }
}

// ================================================================
// 压缩 / 解压 (miniz wrapper，隐藏底层依赖)
// ================================================================

/// Zlib 压缩，返回 true 成功
bool compressData(const uint8_t* src, size_t srcLen, std::vector<uint8_t>& out);

/// Zlib 解压，返回 true 成功
bool decompressData(const uint8_t* src, size_t srcLen,
                    size_t originalSize, std::vector<uint8_t>& out);

// ================================================================
// 密钥碎片 (分散于 3 个翻译单元，防静态分析)
// ================================================================

const uint8_t* getKeyFragment1(size_t& outLen);  // SSECrypto.cpp
const uint8_t* getKeyFragment3(size_t& outLen);  // SSEPakReader.cpp

} // namespace Crypto
} // namespace SimpleScriptEngine

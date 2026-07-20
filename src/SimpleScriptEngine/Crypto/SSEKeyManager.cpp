// ============================================================================
// SSEKeyManager.cpp — 密钥管理器实现
// ============================================================================

#include "SSEKeyManager.h"
#include "SSECrypto.h"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <cstdio>
#include <cstring>

namespace SimpleScriptEngine {
namespace Crypto {

// ================================================================
// 密钥碎片 #2 (分散在 SSEKeyManager.cpp)
// ================================================================

namespace {
    static const uint64_t FRAG2_SEED = 0xB81D4E9A3C7F2650ULL;
    static const uint64_t FRAG2_MASK = 0xF1E2D3C4B5A69708ULL;

    SSE_NOINLINE
    uint64_t computeFragment2() {
        volatile uint64_t x = FRAG2_SEED;
        x ^= FRAG2_MASK;
        x = ((x << 23) | (x >> 41));
        x ^= 0xC3A528BF619E047DULL;
        return x;
    }
}

const uint8_t* KeyManager::getKeyFragment2(size_t& outLen) {
    static uint64_t cached = 0;
    static bool computed = false;

    if (!computed) {
        cached = computeFragment2();
        computed = true;
    }

    outLen = sizeof(uint64_t);
    return reinterpret_cast<const uint8_t*>(&cached);
}

// ================================================================
// 构造 / 单例
// ================================================================

KeyManager::KeyManager() {
    memset(m_cache, 0, sizeof(m_cache));
}

KeyManager& KeyManager::instance() {
    static KeyManager s_instance;
    return s_instance;
}

// ================================================================
// 组装主密钥
// ================================================================

std::vector<uint8_t> KeyManager::assembleMasterKey(uint32_t keyID) {
    // -- 如果用户设了自定义密钥，直接用 --
    if (!customKey_.empty()) {
        std::vector<uint8_t> mk(SSEP_AES_KEY_SIZE);

        // SHA256(customKey_) -> 32 bytes
        unsigned int hashLen = SSEP_AES_KEY_SIZE;
        uint8_t hash[SSEP_AES_KEY_SIZE];
        EVP_MD_CTX* sha = EVP_MD_CTX_new();
        EVP_DigestInit_ex(sha, EVP_sha256(), nullptr);
        EVP_DigestUpdate(sha, customKey_.data(), customKey_.size());
        EVP_DigestFinal_ex(sha, hash, &hashLen);
        EVP_MD_CTX_free(sha);

        // 用 keyID 微调
        for (size_t i = 0; i < SSEP_AES_KEY_SIZE; ++i) {
            mk[i] = hash[i] ^ static_cast<uint8_t>((keyID >> ((i % 4) * 8)) & 0xFF);
        }
        return mk;
    }

    // -- 否则使用内置碎片 --
    size_t len1 = 0, len2 = 0, len3 = 0;
    const uint8_t* frag1 = getKeyFragment1(len1);
    const uint8_t* frag2 = getKeyFragment2(len2);
    const uint8_t* frag3 = getKeyFragment3(len3);

    if (len1 < 8 || len2 < 8 || len3 < 8) {
        uint8_t fallbackKey[SSEP_AES_KEY_SIZE];
        for (int i = 0; i < SSEP_AES_KEY_SIZE; ++i) {
            fallbackKey[i] = static_cast<uint8_t>(
                ((0xD4E17F39B25A0FE6ULL >> ((i % 8) * 8)) & 0xFF) ^
                ((keyID >> ((i % 4) * 8)) & 0xFF) ^
                static_cast<uint8_t>(i * 0x7D));
        }
        return std::vector<uint8_t>(fallbackKey, fallbackKey + SSEP_AES_KEY_SIZE);
    }

    uint64_t k1, k2, k3;
    memcpy(&k1, frag1, 8);
    memcpy(&k2, frag2, 8);
    memcpy(&k3, frag3, 8);

    uint64_t blocks[4];
    blocks[0] = k1 ^ k3;
    blocks[1] = k2 ^ ((k1 << 7) | (k1 >> 57));
    blocks[2] = k3 ^ ((k2 << 13) | (k2 >> 51));
    blocks[3] = k1 ^ k2 ^ k3;

    std::vector<uint8_t> masterKey(SSEP_AES_KEY_SIZE);
    memcpy(masterKey.data(), blocks, SSEP_AES_KEY_SIZE);

    for (size_t i = 0; i < SSEP_AES_KEY_SIZE; ++i) {
        masterKey[i] ^= static_cast<uint8_t>((keyID >> ((i % 4) * 8)) & 0xFF);
        masterKey[i] ^= static_cast<uint8_t>(i * 0x9D);
    }

    return masterKey;
}

// ================================================================
// 自定义密钥注入
// ================================================================

void KeyManager::setCustomKey(const uint8_t* keyData, size_t keyLen) {
    customKey_.assign(keyData, keyData + keyLen);
    // 清除缓存 (旧密钥已失效)
    memset(m_cache, 0, sizeof(m_cache));
}

bool KeyManager::setCustomKeyHex(const char* hexString) {
    if (!hexString) return false;
    size_t len = strlen(hexString);
    if (len < 32 || len % 2 != 0) return false; // 至少 16 字节

    std::vector<uint8_t> key(len / 2);
    for (size_t i = 0; i < len; i += 2) {
        unsigned int byte;
        if (sscanf(hexString + i, "%2x", &byte) != 1) return false;
        key[i / 2] = static_cast<uint8_t>(byte);
    }

    setCustomKey(key.data(), key.size());
    return true;
}

void KeyManager::setCustomPassphrase(const char* passphrase) {
    if (!passphrase) return;
    setCustomKey(reinterpret_cast<const uint8_t*>(passphrase), strlen(passphrase));
}

// ================================================================
// 密钥派生
// ================================================================

bool KeyManager::deriveKeys(uint64_t pakUUID, uint32_t keyID,
                             uint8_t* keyOut, uint8_t* ivOut)
{
    // 检查缓存
    for (int i = 0; i < MAX_CACHED; ++i) {
        if (m_cache[i].valid &&
            m_cache[i].pakUUID == pakUUID &&
            m_cache[i].keyID == keyID)
        {
            memcpy(keyOut, m_cache[i].aesKey, SSEP_AES_KEY_SIZE);
            memcpy(ivOut, m_cache[i].aesIV, SSEP_GCM_IV_SIZE);
            return true;
        }
    }

    // 组装主密钥
    std::vector<uint8_t> masterKey = assembleMasterKey(keyID);

    // HKDF 派生
    uint8_t salt[8];
    memcpy(salt, &pakUUID, 8);

    if (!HKDF::deriveKeyAndIV(masterKey, salt, sizeof(salt),
                              "SSEP-v1-AES256", keyOut, ivOut))
    {
        secureZero(masterKey);
        return false;
    }

    secureZero(masterKey);

    // 缓存
    for (int i = 0; i < MAX_CACHED; ++i) {
        if (!m_cache[i].valid) {
            m_cache[i].pakUUID = pakUUID;
            m_cache[i].keyID   = keyID;
            memcpy(m_cache[i].aesKey, keyOut, SSEP_AES_KEY_SIZE);
            memcpy(m_cache[i].aesIV, ivOut, SSEP_GCM_IV_SIZE);
            m_cache[i].valid = true;
            break;
        }
    }

    return true;
}

bool KeyManager::deriveKeys(const SSEPakHeader& header,
                             uint8_t* keyOut, uint8_t* ivOut)
{
    return deriveKeys(header.pakUUID, header.keyID, keyOut, ivOut);
}

// ================================================================
// 随机数生成
// ================================================================

void KeyManager::generateIV(uint8_t* ivOut) {
    RAND_bytes(ivOut, SSEP_GCM_IV_SIZE);
}

uint64_t KeyManager::generateUUID() {
    uint64_t uuid = 0;
    RAND_bytes(reinterpret_cast<uint8_t*>(&uuid), sizeof(uuid));
    return uuid;
}

} // namespace Crypto
} // namespace SimpleScriptEngine

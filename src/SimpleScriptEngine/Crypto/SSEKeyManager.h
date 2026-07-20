#pragma once
// ============================================================================
// SSEKeyManager.h -- Key Manager (supports custom key injection)
// ============================================================================

#include "SSEPakFormat.h"

#include <cstdint>
#include <vector>

namespace SimpleScriptEngine {
namespace Crypto {

class KeyManager {
public:
    static KeyManager& instance();

    // ---- Custom key injection ----

    /// Set a user-provided master key (overrides built-in key fragments).
    /// Must be called before any deriveKeys() call.
    /// Key must be >= 16 bytes, will be hashed to 32 bytes via SHA256.
    void setCustomKey(const uint8_t* keyData, size_t keyLen);

    /// Convenience: set custom key from hex string (64 hex chars = 32 bytes)
    bool setCustomKeyHex(const char* hexString);

    /// Convenience: set custom key from passphrase string
    void setCustomPassphrase(const char* passphrase);

    // ---- Key derivation ----

    bool deriveKeys(uint64_t pakUUID, uint32_t keyID,
                    uint8_t* keyOut, uint8_t* ivOut);

    bool deriveKeys(const SSEPakHeader& header,
                    uint8_t* keyOut, uint8_t* ivOut);

    // ---- Random generation ----

    static void generateIV(uint8_t* ivOut);
    static uint64_t generateUUID();

private:
    KeyManager();
    std::vector<uint8_t> assembleMasterKey(uint32_t keyID);
    static const uint8_t* getKeyFragment2(size_t& outLen);

    // Custom key (empty = use built-in fragments)
    std::vector<uint8_t> customKey_;

    // Cache
    static const int MAX_CACHED = 8;
    struct CachedKey {
        uint64_t pakUUID;
        uint32_t keyID;
        uint8_t  aesKey[SSEP_AES_KEY_SIZE];
        uint8_t  aesIV[SSEP_GCM_IV_SIZE];
        bool     valid;
    };
    CachedKey m_cache[MAX_CACHED];
};

} // namespace Crypto
} // namespace SimpleScriptEngine

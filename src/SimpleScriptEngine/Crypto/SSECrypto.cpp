// ============================================================================
// SSECrypto.cpp — AES-256-GCM + HMAC-SHA256 + HKDF 实现 (OpenSSL EVP)
// ============================================================================

#include "SSECrypto.h"

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

#include "miniz.h"

#include <cstring>
#include <stdexcept>

namespace SimpleScriptEngine {
namespace Crypto {

// ================================================================
// 内部辅助
// ================================================================

namespace {

// RAII EVP_CIPHER_CTX 封装
struct EVPCipherCtx {
    EVP_CIPHER_CTX* ctx = nullptr;

    EVPCipherCtx() {
        ctx = EVP_CIPHER_CTX_new();
        if (!ctx) throw std::runtime_error("EVP_CIPHER_CTX_new failed");
    }

    ~EVPCipherCtx() {
        if (ctx) EVP_CIPHER_CTX_free(ctx);
    }

    EVP_CIPHER_CTX* get() { return ctx; }

    EVPCipherCtx(const EVPCipherCtx&) = delete;
    EVPCipherCtx& operator=(const EVPCipherCtx&) = delete;
};

} // anonymous namespace

// ================================================================
// AES-256-GCM 加密
// ================================================================

std::vector<uint8_t> AESCipher::encrypt(
    const std::vector<uint8_t>& plaintext,
    const uint8_t* key,
    const uint8_t* iv,
    uint8_t* tag_out)
{
    if (plaintext.empty()) {
        // GCM 对空明文也需生成 tag
        EVPCipherCtx ctx;
        EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, (int)SSEP_GCM_IV_SIZE, nullptr);
        EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr, key, iv);

        int len = 0;
        EVP_EncryptUpdate(ctx.get(), nullptr, &len, nullptr, 0);
        EVP_EncryptFinal_ex(ctx.get(), nullptr, &len);
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, (int)SSEP_GCM_TAG_SIZE, tag_out);
        return {};
    }

    EVPCipherCtx ctx;
    EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, (int)SSEP_GCM_IV_SIZE, nullptr);
    EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr, key, iv);

    std::vector<uint8_t> ciphertext(plaintext.size());
    int outLen = 0, totalLen = 0;

    EVP_EncryptUpdate(ctx.get(), ciphertext.data(), &outLen,
                      plaintext.data(), (int)plaintext.size());
    totalLen = outLen;

    EVP_EncryptFinal_ex(ctx.get(), ciphertext.data() + totalLen, &outLen);
    totalLen += outLen;
    ciphertext.resize((size_t)totalLen);

    EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, (int)SSEP_GCM_TAG_SIZE, tag_out);
    return ciphertext;
}

// ================================================================
// AES-256-GCM 解密
// ================================================================

std::vector<uint8_t> AESCipher::decrypt(
    const std::vector<uint8_t>& ciphertext,
    const uint8_t* key,
    const uint8_t* iv,
    const uint8_t* tag)
{
    if (ciphertext.empty()) {
        EVPCipherCtx ctx;
        EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, (int)SSEP_GCM_IV_SIZE, nullptr);
        EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr, key, iv);

        int len = 0;
        EVP_DecryptUpdate(ctx.get(), nullptr, &len, nullptr, 0);
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, (int)SSEP_GCM_TAG_SIZE,
                           (void*)tag);

        int ret = EVP_DecryptFinal_ex(ctx.get(), nullptr, &len);
        return (ret > 0) ? std::vector<uint8_t>() : std::vector<uint8_t>();
    }

    EVPCipherCtx ctx;
    EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, (int)SSEP_GCM_IV_SIZE, nullptr);
    EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr, key, iv);

    std::vector<uint8_t> plaintext(ciphertext.size());
    int outLen = 0, totalLen = 0;

    EVP_DecryptUpdate(ctx.get(), plaintext.data(), &outLen,
                      ciphertext.data(), (int)ciphertext.size());
    totalLen = outLen;

    EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, (int)SSEP_GCM_TAG_SIZE, (void*)tag);

    int ret = EVP_DecryptFinal_ex(ctx.get(), plaintext.data() + totalLen, &outLen);

    if (ret <= 0) {
        secureZero(plaintext);
        return {};
    }

    totalLen += outLen;
    plaintext.resize((size_t)totalLen);
    return plaintext;
}

// ================================================================
// HMAC-SHA256
// ================================================================

std::vector<uint8_t> HMACEngine::compute(
    const uint8_t* data, size_t dataLen,
    const uint8_t* key, size_t keyLen)
{
    std::vector<uint8_t> result(SSEP_HMAC_SIZE);
    unsigned int resultLen = (unsigned int)SSEP_HMAC_SIZE;

    unsigned char* hmac = HMAC(
        EVP_sha256(),
        key, (int)keyLen,
        data, dataLen,
        result.data(), &resultLen);

    if (!hmac || resultLen != SSEP_HMAC_SIZE) {
        return {};
    }

    return result;
}

bool HMACEngine::verify(
    const uint8_t* data, size_t dataLen,
    const uint8_t* key, size_t keyLen,
    const uint8_t* expectedHMAC)
{
    std::vector<uint8_t> computed = compute(data, dataLen, key, keyLen);
    if (computed.size() != SSEP_HMAC_SIZE) return false;

    int result = 0;
    for (size_t i = 0; i < SSEP_HMAC_SIZE; ++i) {
        result |= (computed[i] ^ expectedHMAC[i]);
    }
    return result == 0;
}

// ================================================================
// HKDF-SHA256 密钥派生 (RFC 5869, 手动实现)
// ================================================================

bool HKDF::deriveKeyAndIV(
    const std::vector<uint8_t>& masterKey,
    const uint8_t* salt, size_t saltLen,
    const char* info,
    uint8_t* keyOut,
    uint8_t* ivOut)
{
    // HKDF-Extract: PRK = HMAC-SHA256(salt, IKM)
    uint8_t prk[SSEP_HMAC_SIZE];
    unsigned int prkLen = SSEP_HMAC_SIZE;

    // 如果没有 salt，使用全零 salt (RFC 5869 要求)
    uint8_t zeroSalt[SSEP_HMAC_SIZE] = {};
    if (!salt || saltLen == 0) {
        salt = zeroSalt;
        saltLen = SSEP_HMAC_SIZE;
    }

    if (!HMAC(EVP_sha256(), salt, (int)saltLen,
              masterKey.data(), masterKey.size(),
              prk, &prkLen) || prkLen != SSEP_HMAC_SIZE)
    {
        secureZero(prk, sizeof(prk));
        return false;
    }

    // HKDF-Expand: OKM = T(1) || T(2) || ...
    // T(i) = HMAC-SHA256(PRK, T(i-1) || info || i)
    // 需要 44 字节 = 32 (key) + 12 (IV), 需要 2 轮

    size_t infoLen = (info && info[0]) ? strlen(info) : 0;
    size_t needed = SSEP_AES_KEY_SIZE + SSEP_GCM_IV_SIZE;  // 44

    std::vector<uint8_t> okm;
    okm.reserve(64); // 2 * 32

    std::vector<uint8_t> prev;  // T(i-1), 初始为空

    for (uint8_t counter = 1; okm.size() < needed; ++counter) {
        unsigned int outLen = SSEP_HMAC_SIZE;
        uint8_t mac[SSEP_HMAC_SIZE];

        // 使用 OpenSSL 1.1 HMAC API (兼容 1.1 / 3.0)
        HMAC_CTX* hctx = HMAC_CTX_new();
        HMAC_Init_ex(hctx, prk, SSEP_HMAC_SIZE, EVP_sha256(), nullptr);

        if (!prev.empty()) {
            HMAC_Update(hctx, prev.data(), prev.size());
        }
        if (infoLen > 0) {
            HMAC_Update(hctx, (const uint8_t*)info, infoLen);
        }
        HMAC_Update(hctx, &counter, 1);
        HMAC_Final(hctx, mac, &outLen);
        HMAC_CTX_free(hctx);
        if (outLen != SSEP_HMAC_SIZE) {
            secureZero(prk, sizeof(prk));
            secureZero(okm);
            return false;
        }

        okm.insert(okm.end(), mac, mac + SSEP_HMAC_SIZE);
        prev.assign(mac, mac + SSEP_HMAC_SIZE);
    }

    secureZero(prk, sizeof(prk));

    if (okm.size() < needed) {
        secureZero(okm);
        return false;
    }

    memcpy(keyOut, okm.data(), SSEP_AES_KEY_SIZE);
    memcpy(ivOut, okm.data() + SSEP_AES_KEY_SIZE, SSEP_GCM_IV_SIZE);

    secureZero(okm);
    return true;
}

// ================================================================
// CRC32
// ================================================================

uint32_t crc32(const uint8_t* data, size_t len) {
    static const uint32_t table[256] = {
        0x00000000,0x77073096,0xEE0E612C,0x990951BA,0x076DC419,0x706AF48F,
        0xE963A535,0x9E6495A3,0x0EDB8832,0x79DCB8A4,0xE0D5E91E,0x97D2D988,
        0x09B64C2B,0x7EB17CBD,0xE7B82D07,0x90BF1D91,0x1DB71064,0x6AB020F2,
        0xF3B97148,0x84BE41DE,0x1ADAD47D,0x6DDDE4EB,0xF4D4B551,0x83D385C7,
        0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,0x14015C4F,0x63066CD9,
        0xFA0F3D63,0x8D080DF5,0x3B6E20C8,0x4C69105E,0xD56041E4,0xA2677172,
        0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,0x35B5A8FA,0x42B2986C,
        0xDBBBC9D6,0xACBCF940,0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,
        0x26D930AC,0x51DE003A,0xC8D75180,0xBFD06116,0x21B4F4B5,0x56B3C423,
        0xCFBA9599,0xB8BDA50F,0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,
        0x2F6F7C87,0x58684C11,0xC1611DAB,0xB6662D3D,0x76DC4190,0x01DB7106,
        0x98D220BC,0xEFD5102A,0x71B18589,0x06B6B51F,0x9FBFE4A5,0xE8B8D433,
        0x7807C9A2,0x0F00F934,0x9609A88E,0xE10E9818,0x7F6A0DBB,0x086D3D2D,
        0x91646C97,0xE6635C01,0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,
        0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,0x65B0D9C6,0x12B7E950,
        0x8BBEB8EA,0xFCB9887C,0x62DD1DDF,0x15DA2D49,0x8CD37CF3,0xFBD44C65,
        0x4DB26158,0x3AB551CE,0xA3BC0074,0xD4BB30E2,0x4ADFA541,0x3DD895D7,
        0xA4D1C46D,0xD3D6F4FB,0x4369E96A,0x346ED9FC,0xAD678846,0xDA60B8D0,
        0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,0x5005713C,0x270241AA,
        0xBE0B1010,0xC90C2086,0x5768B525,0x206F85B3,0xB966D409,0xCE61E49F,
        0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,0x59B33D17,0x2EB40D81,
        0xB7BD5C3B,0xC0BA6CAD,0xEDB88320,0x9ABFB3B6,0x03B6E20C,0x74B1D29A,
        0xEAD54739,0x9DD277AF,0x04DB2615,0x73DC1683,0xE3630B12,0x94643B84,
        0x0D6D6A3E,0x7A6A5AA8,0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,
        0xF00F9344,0x8708A3D2,0x1E01F268,0x6906C2FE,0xF762575D,0x806567CB,
        0x196C3671,0x6E6B06E7,0xFED41B76,0x89D32BE0,0x10DA7A5A,0x67DD4ACC,
        0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,0xD6D6A3E8,0xA1D1937E,
        0x38D8C2C4,0x4FDFF252,0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,
        0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,0xDF60EFC3,0xA867DF55,
        0x316E8EEF,0x4669BE79,0xCB61B38C,0xBC66831A,0x256FD2A0,0x5268E236,
        0xCC0C7795,0xBB0B4703,0x220216B9,0x5505262F,0xC5BA3BBE,0xB2BD0B28,
        0x2BB45A92,0x5CB36A04,0xC2D7FFA7,0xB5D0CF31,0x2CD99E8B,0x5BDEAE1D,
        0x9B64C2B0,0xEC63F226,0x756AA39C,0x026D930A,0x9C0906A9,0xEB0E363F,
        0x72076785,0x05005713,0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,
        0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,0x0BDBDF21,0x86D3D2D4,0xF1D4E242,
        0x68DDB3F8,0x1FDA836E,0x81BE16CD,0xF6B9265B,0x6FB077E1,0x18B74777,
        0x88085AE6,0xFF0F6A70,0x66063BCA,0x11010B5C,0x8F659EFF,0xF862AE69,
        0x616BFFD3,0x166CCF45,0xA00AE278,0xD70DD2EE,0x4E048354,0x3903B3C2,
        0xA7672661,0xD06016F7,0x4969474D,0x3E6E77DB,0xAED16A4A,0xD9D65ADC,
        0x40DF0B66,0x37D83BF0,0xA9BCAE53,0xDEBB9EC5,0x47B2CF7F,0x30B5FFE9,
        0xBDBDF21C,0xCABAC28A,0x53B39330,0x24B4A3A6,0xBAD03605,0xCDD70693,
        0x54DE5729,0x23D967BF,0xB3667A2E,0xC4614AB8,0x5D681B02,0x2A6F2B94,
        0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D
    };

    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

// ================================================================
// 安全内存擦除
// ================================================================

void secureZero(void* ptr, size_t len) {
    if (ptr && len > 0) {
        volatile uint8_t* p = static_cast<volatile uint8_t*>(ptr);
        while (len--) {
            *p++ = 0;
        }
#if defined(_MSC_VER)
        _ReadWriteBarrier();
#else
        __asm__ __volatile__("" ::: "memory");
#endif
    }
}

// ================================================================
// 压缩 / 解压 (miniz wrapper)
// ================================================================

bool compressData(const uint8_t* src, size_t srcLen, std::vector<uint8_t>& out) {
    if (!src || srcLen == 0) return false;
    mz_ulong destLen = mz_compressBound(static_cast<mz_ulong>(srcLen));
    out.resize(destLen);

    int ret = mz_compress(out.data(), &destLen, src, static_cast<mz_ulong>(srcLen));
    if (ret != MZ_OK) {
        out.clear();
        return false;
    }

    out.resize(destLen);
    return true;
}

bool decompressData(const uint8_t* src, size_t srcLen,
                    size_t originalSize, std::vector<uint8_t>& out) {
    if (!src || srcLen == 0) return false;
    out.resize(originalSize);
    mz_ulong destLen = static_cast<mz_ulong>(originalSize);

    int ret = mz_uncompress(out.data(), &destLen, src, static_cast<mz_ulong>(srcLen));
    if (ret != MZ_OK) {
        secureZero(out);
        return false;
    }

    out.resize(destLen);
    return true;
}

// ================================================================
// 密钥碎片 #1 (分散在 SSECrypto.cpp)
// ================================================================

namespace {
    static const uint64_t FRAG1_SEED = 0x7A3F5C8D2E1B9064ULL;
    static const uint64_t FRAG1_MASK = 0xA1B2C3D4E5F60718ULL;

    SSE_NOINLINE
    uint64_t computeFragment1() {
        volatile uint64_t x = FRAG1_SEED;
        x ^= FRAG1_MASK;
        x = ((x << 17) | (x >> 47));
        x ^= 0x9E3779B97F4A7C15ULL;
        return x;
    }
}

const uint8_t* getKeyFragment1(size_t& outLen) {
    static uint64_t cached = 0;
    static bool computed = false;

    if (!computed) {
        cached = computeFragment1();
        computed = true;
    }

    outLen = sizeof(uint64_t);
    return reinterpret_cast<const uint8_t*>(&cached);
}

} // namespace Crypto
} // namespace SimpleScriptEngine

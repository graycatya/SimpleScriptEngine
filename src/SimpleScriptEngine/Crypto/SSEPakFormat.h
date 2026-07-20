#pragma once
// ============================================================================
// SSEPakFormat.h — SSEP 二进制包格式定义
// ============================================================================
// SSEP (SimpleScriptEngine Protected Package) V1
//
// 文件布局:
//   [Header 128B cleartext] [Encrypted: Index + Data + Trailer]
//   加密方案: AES-256-GCM (认证加密，防篡改)
//   压缩方案: Zlib (加密前压缩)
//
// ============================================================================

#include <cstdint>

namespace SimpleScriptEngine {
namespace Crypto {

// ================================================================
// 常量
// ================================================================

constexpr uint32_t SSEP_MAGIC           = 0x50455353;  // "SSEP" (little-endian)
constexpr uint16_t SSEP_VERSION         = 1;
constexpr size_t   SSEP_HEADER_SIZE     = 128;
constexpr size_t   SSEP_GCM_TAG_SIZE    = 16;          // AES-GCM auth tag
constexpr size_t   SSEP_GCM_IV_SIZE     = 12;          // 96-bit nonce
constexpr size_t   SSEP_AES_KEY_SIZE    = 32;          // AES-256
constexpr size_t   SSEP_HMAC_SIZE       = 32;          // SHA-256 HMAC

// 标志位
constexpr uint16_t SSEP_FLAG_COMPRESSED  = 0x0001;      // Zlib 压缩
constexpr uint16_t SSEP_FLAG_ENCRYPTED   = 0x0002;      // AES-256-GCM 加密
constexpr uint16_t SSEP_FLAG_SIGNED      = 0x0004;      // HMAC 签名
constexpr uint16_t SSEP_FLAG_CUSTOM_KEY  = 0x0010;      // 使用了用户自定义密钥

// ================================================================
// 引擎类型枚举
// ================================================================

enum class EngineType : uint8_t {
    LuaJIT      = 0,
    QuickJS     = 1,
    ChaiScript  = 2,
    AngelScript = 3,
    Unknown     = 0xFF
};

inline const char* engineTypeName(EngineType t) {
    switch (t) {
        case EngineType::LuaJIT:      return "LuaJIT";
        case EngineType::QuickJS:     return "QuickJS";
        case EngineType::ChaiScript:  return "ChaiScript";
        case EngineType::AngelScript: return "AngelScript";
        default:                      return "Unknown";
    }
}

// ================================================================
// SSEP Header (128 bytes, cleartext, packed)
// ================================================================

#pragma pack(push, 1)
struct SSEPakHeader {
    uint32_t magic;              // +0   SSEP_MAGIC
    uint16_t version;            // +4   版本号
    uint16_t flags;              // +6   标志位
    uint64_t pakUUID;            // +8   包唯一 ID (作为 HKDF salt)
    uint32_t keyID;              // +16  密钥槽位
    uint32_t fileCount;          // +20  文件数量
    uint64_t indexSize;          // +24  索引段大小 (压缩前)
    uint64_t dataSize;           // +32  数据段总大小 (压缩后)
    uint64_t originalSize;       // +40  所有文件原始总大小
    uint32_t headerCRC;          // +48  头部 CRC32 (不含本字段及之后)
    uint8_t  iv[SSEP_GCM_IV_SIZE]; // +52  AES-GCM IV (前12B, 后4B由counter填充)
    uint8_t  reserved[64];       // +64  保留
};
#pragma pack(pop)

static_assert(sizeof(SSEPakHeader) == SSEP_HEADER_SIZE,
              "SSEPakHeader must be exactly 128 bytes");

// ================================================================
// 文件索引条目 (在加密区内)
// ================================================================

#pragma pack(push, 1)
struct SSEFileEntry {
    uint8_t  engineType;         // 引擎类型
    uint64_t nameHash;           // 文件名 xxHash64 (用于快速查找)
    uint16_t nameLen;            // 文件名长度
    // char name[nameLen];       // 文件名 (UTF-8, 可变长)
    uint64_t dataOffset;         // 数据在数据段的偏移
    uint64_t compressedSize;     // 压缩后大小
    uint64_t originalSize;       // 原始大小
    uint32_t originalCRC;        // 原始数据 CRC32
};
#pragma pack(pop)

static_assert(sizeof(SSEFileEntry) == 39,
              "SSEFileEntry must be 39 bytes (before name)");

// ================================================================
// 加密段 Trailer (在加密区内，紧跟数据段)
// ================================================================

#pragma pack(push, 1)
struct SSEPakTrailer {
    uint8_t indexHMAC[SSEP_HMAC_SIZE];   // 索引段 HMAC-SHA256
    uint8_t dataHMAC[SSEP_HMAC_SIZE];    // 数据段 HMAC-SHA256
    uint8_t gcmTag[SSEP_GCM_TAG_SIZE];   // AES-GCM 认证标签
};
#pragma pack(pop)

static_assert(sizeof(SSEPakTrailer) == SSEP_HMAC_SIZE * 2 + SSEP_GCM_TAG_SIZE);

} // namespace Crypto
} // namespace SimpleScriptEngine

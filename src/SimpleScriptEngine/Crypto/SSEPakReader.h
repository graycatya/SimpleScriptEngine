#pragma once
// ============================================================================
// SSEPakReader.h — .ssep 包运行时读取器
// ============================================================================
// 职责:
//   1. 打开 .ssep 文件，解析 Header
//   2. 解密索引段 + 数据段
//   3. 按文件名查找并提取单个文件数据
//   4. 所有解密数据仅在内存中，用完立即擦除
//
// 线程安全: 实例本身不是线程安全的，调用方负责同步
// ============================================================================

#include "SSEPakFormat.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace SimpleScriptEngine {
namespace Crypto {

// ================================================================
// 解密后的文件数据 (RAII 自擦除)
// ================================================================

struct SecureBuffer {
    std::vector<uint8_t> data;

    SecureBuffer() = default;
    explicit SecureBuffer(std::vector<uint8_t>&& d) : data(std::move(d)) {}
    ~SecureBuffer() { wipe(); }

    // 不可拷贝
    SecureBuffer(const SecureBuffer&) = delete;
    SecureBuffer& operator=(const SecureBuffer&) = delete;

    // 可移动
    SecureBuffer(SecureBuffer&& other) noexcept : data(std::move(other.data)) {}
    SecureBuffer& operator=(SecureBuffer&& other) noexcept {
        if (this != &other) {
            wipe();
            data = std::move(other.data);
        }
        return *this;
    }

    void wipe();

    const uint8_t* ptr()  const { return data.data(); }
    size_t         size() const { return data.size(); }
    bool           empty() const { return data.empty(); }
};

// ================================================================
// 文件索引条目 (解密后的)
// ================================================================

struct IndexEntry {
    EngineType engineType     = EngineType::Unknown;
    uint64_t   nameHash       = 0;
    std::string fileName;
    uint64_t   dataOffset     = 0;   // 数据段内的偏移
    uint64_t   compressedSize = 0;
    uint64_t   originalSize   = 0;
    uint32_t   originalCRC    = 0;
};

// ================================================================
// SSEPakReader
// ================================================================

class SSEPakReader {
public:
    SSEPakReader();
    ~SSEPakReader();

    // 禁止拷贝
    SSEPakReader(const SSEPakReader&) = delete;
    SSEPakReader& operator=(const SSEPakReader&) = delete;

    // ============================================================
    // 生命周期
    // ============================================================

    /// 打开并解析 .ssep 文件
    /// @return true 成功 (含完整性校验通过)
    bool open(const std::string& path);

    /// 关闭并释放所有资源
    void close();

    /// 是否已成功打开
    bool isOpen() const { return open_; }

    // ============================================================
    // 文件访问
    // ============================================================

    /// 按文件名获取解密解压后的文件数据
    /// @param fileName 脚本文件名 (如 "main.lua")
    /// @return 解密后的数据，失败返回空 SecureBuffer
    SecureBuffer getFile(const std::string& fileName);

    /// 列出包内所有文件
    std::vector<std::string> listFiles() const;

    /// 包内文件数量
    uint32_t fileCount() const { return header_.fileCount; }

    /// 是否使用了自定义密钥（不需要完整加载）
    bool needsCustomKey() const { return (header_.flags & SSEP_FLAG_CUSTOM_KEY) != 0; }

    /// 轻量静态方法：仅读 Header 判断是否需要自定义密钥
    /// @return true 该包需要用户提供密钥
    static bool pakNeedsCustomKey(const std::string& path);

    /// 包的 UUID
    uint64_t pakUUID() const { return header_.pakUUID; }

    // ============================================================
    // 批量操作
    // ============================================================

    /// 批量提取所有文件
    /// @param callback 每提取一个文件调用一次
    ///   (const IndexEntry& entry, SecureBuffer&& data)
    template<typename Callback>
    void extractAll(Callback&& callback);

private:
    bool open_ = false;

    SSEPakHeader header_;
    std::vector<IndexEntry> index_;

    // 文件映射 / 文件数据
    std::vector<uint8_t> rawData_;     // 加密段原始数据
    std::vector<uint8_t> decryptedData_; // 解密后的数据段

    // 解密
    bool decryptSection();

    // 解析索引
    bool parseIndex();

    // Zlib 解压
    static std::vector<uint8_t> decompress(const uint8_t* data, size_t size,
                                            size_t originalSize);

    // 文件验证
    bool verifyIntegrity();
};

// ================================================================
// 模板实现
// ================================================================

template<typename Callback>
void SSEPakReader::extractAll(Callback&& callback) {
    for (const auto& entry : index_) {
        auto buf = getFile(entry.fileName);
        if (!buf.empty()) {
            callback(entry, std::move(buf));
        }
    }
}

} // namespace Crypto
} // namespace SimpleScriptEngine

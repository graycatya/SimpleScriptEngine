# ============================================================================
# SSEPakBuilder — .ssep 包构建工具
# ============================================================================
# 依赖 SimpleScriptEngine 库 (获取 Crypto 模块符号),
# 不再重复编译 Crypto 源文件。
# ============================================================================

cmake_minimum_required(VERSION 3.16)
project(SSEPakBuilder LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- 源文件 (仅 main.cpp, Crypto 来自 SimpleScriptEngine 库) ----

add_executable(SSEPakBuilder main.cpp)

# ---- Include ----

target_include_directories(SSEPakBuilder PRIVATE
    ${CMAKE_SOURCE_DIR}/src/SimpleScriptEngine
    ${CMAKE_SOURCE_DIR}/src/SimpleScriptEngine/Crypto
)

# ---- 链接 ----
# SimpleScriptEngine 库提供所有 Crypto 符号,
# 但 OpenSSL 是 PRIVATE 链接, 这里需要显式加上

target_link_libraries(SSEPakBuilder PRIVATE
    SimpleScriptEngine
    OpenSSL::Crypto
    miniz
)

# ---- 编译选项 ----

if(MSVC)
    target_compile_options(SSEPakBuilder PRIVATE /utf-8 /bigobj)
endif()

message(STATUS "[SSEPakBuilder] Build configured (links SimpleScriptEngine)")

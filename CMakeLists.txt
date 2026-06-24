cmake_minimum_required(VERSION 3.16)

project(SimpleScriptEngine-Porject
    VERSION 0.1.0
    DESCRIPTION "Quad-Engine — 4 Script Engines in One C++ Project"
    LANGUAGES C CXX
)

# ============================================================================
# Engine build options
# ============================================================================
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
option(SIMPLESCRIPTENGINE_ENABLE_LUAJIT      "Enable LuaJIT engine"        ON)
option(SIMPLESCRIPTENGINE_ENABLE_QUICKJS     "Enable QuickJS engine"       ON)
option(SIMPLESCRIPTENGINE_ENABLE_CHAISCRIPT  "Enable ChaiScript engine"    ON)
option(SIMPLESCRIPTENGINE_ENABLE_ANGELSCRIPT "Enable AngelScript engine"   ON)

option(SIMPLESCRIPTENGINE_BUILD_EXAMPLES     "Build example programs"      ON)
option(SIMPLESCRIPTENGINE_BUILD_SHARED       "Build as shared library"     OFF)

option(ENABLE_PROXY "Enable proxy for network requests" OFF)
set(DEFAULT_HTTP_PROXY "" CACHE STRING "Default HTTP proxy URL")
set(DEFAULT_HTTPS_PROXY "" CACHE STRING "Default HTTPS proxy URL")

if(ENABLE_PROXY)
    if(DEFINED HTTP_PROXY OR DEFINED HTTPS_PROXY)
        if(DEFINED HTTP_PROXY)
            set(ENV{HTTP_PROXY} ${HTTP_PROXY})
            message(STATUS "Using HTTP proxy from CMake variable: ${HTTP_PROXY}")
        endif()
        if(DEFINED HTTPS_PROXY)
            set(ENV{HTTPS_PROXY} ${HTTPS_PROXY})
            message(STATUS "Using HTTPS proxy from CMake variable: ${HTTPS_PROXY}")
        endif()
    elseif(DEFINED ENV{HTTP_PROXY} OR DEFINED ENV{HTTPS_PROXY})
        if(DEFINED ENV{HTTP_PROXY})
            set(ENV{HTTP_PROXY} $ENV{HTTP_PROXY})
            message(STATUS "Using HTTP proxy from environment: $ENV{HTTP_PROXY}")
        endif()
        if(DEFINED ENV{HTTPS_PROXY})
            set(ENV{HTTPS_PROXY} $ENV{HTTPS_PROXY})
            message(STATUS "Using HTTPS proxy from environment: $ENV{HTTPS_PROXY}")
        endif()
    else()
        set(ENV{HTTP_PROXY} ${DEFAULT_HTTP_PROXY})
        set(ENV{HTTPS_PROXY} ${DEFAULT_HTTPS_PROXY})
        message(STATUS "Using default proxies: HTTP=${DEFAULT_HTTP_PROXY}, HTTPS=${DEFAULT_HTTPS_PROXY}")
    endif()
else()
    message(STATUS "Proxy disabled")
endif()


# ============================================================================
# C++ Standard
# ============================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 解决 MSVC C4819 警告（中文注释使用 UTF-8 编码）
if(MSVC)
    add_compile_options(/utf-8)
endif()

# ============================================================================
# Third-party dependency directories
# ============================================================================

set(3RDPARTY_PATH ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty)
set(SRC_MODULES_PATH ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(3RDPARTY_ZIP_PATH ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty-download)



# ============================================================================
# Include cmake modules (download / extract macros)
# ============================================================================

include(DownloadUnzipProject)
include(ExecuteDownloadProjects)



# ============================================================================
# 核心库 — SimpleScriptEngine Core
# ============================================================================

if(SIMPLESCRIPTENGINE_ENABLE_LUAJIT)
    set(WITH_LUAJIT21 ON CACHE BOOL "BUILD_LUAJIT21 ON" FORCE)
    # LUAJIT_SYSTEM_BUILDVM should be a PATH to a prebuilt buildvm/luajit binary, not a boolean.
    # Leave it unset so buildvm is compiled from source.
    #set(LUAJIT_SYSTEM_MINILUA ON CACHE BOOL "LUAJIT_SYSTEM_MINILUA ON" FORCE)
    add_subdirectory(${3RDPARTY_PATH}/luajit-rocks)
    message(STATUS "[SIMPLESCRIPTENGINE] LuaJIT enabled")
endif()

if(SIMPLESCRIPTENGINE_ENABLE_QUICKJS)
    set(EMSCRIPTEN OFF CACHE BOOL "EMSCRIPTEN OFF" FORCE)
    add_subdirectory(${3RDPARTY_PATH}/quickjs EXCLUDE_FROM_ALL)
    message(STATUS "[SIMPLESCRIPTENGINE] QuickJS enabled")
endif()

if(SIMPLESCRIPTENGINE_ENABLE_CHAISCRIPT)
    set(UNIT_TEST_LIGHT ON CACHE BOOL "UNIT_TEST_LIGHT ON" FORCE)
    add_subdirectory(${3RDPARTY_PATH}/ChaiScript)
    message(STATUS "[SIMPLESCRIPTENGINE] ChaiScript enabled")
endif()

if(SIMPLESCRIPTENGINE_ENABLE_ANGELSCRIPT)
    add_subdirectory(${3RDPARTY_PATH}/angelscript/sdk/angelscript/projects/cmake)
    message(STATUS "[SIMPLESCRIPTENGINE] AngelScript enabled")
endif()


# ============================================================================
# 核心库 — SimpleScriptEngine
# ============================================================================

add_subdirectory(src/SimpleScriptEngine)
message(STATUS "[SIMPLESCRIPTENGINE] Core library added")

# ============================================================================
# Examples
# ============================================================================

if(SIMPLESCRIPTENGINE_BUILD_EXAMPLES)
    add_subdirectory(examples/SimpleScriptEngineExample)
    message(STATUS "[SIMPLESCRIPTENGINE] Example added")
endif()

# ============================================================================
# Configuration summary
# ============================================================================

message(STATUS "=============================================================================")
message(STATUS "[SimpleScriptEngine] Configuration Summary")
message(STATUS "  Version:             ${PROJECT_VERSION}")
message(STATUS "  C++ Standard:        C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Build type:          ${CMAKE_BUILD_TYPE}")
message(STATUS "  Shared library:      ${SIMPLESCRIPTENGINE_BUILD_SHARED}")
message(STATUS "")
message(STATUS "  Engines:")
message(STATUS "    LuaJIT:            ${SIMPLESCRIPTENGINE_ENABLE_LUAJIT}")
message(STATUS "    QuickJS:           ${SIMPLESCRIPTENGINE_ENABLE_QUICKJS}")
message(STATUS "    ChaiScript:        ${SIMPLESCRIPTENGINE_ENABLE_CHAISCRIPT}")
message(STATUS "    AngelScript:       ${SIMPLESCRIPTENGINE_ENABLE_ANGELSCRIPT}")
message(STATUS "")
message(STATUS "  Examples:            ${SIMPLESCRIPTENGINE_BUILD_EXAMPLES}")
message(STATUS "  3rdparty path:       ${3RDPARTY_PATH}")
message(STATUS "=============================================================================")

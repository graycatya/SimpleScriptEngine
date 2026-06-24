# ============================================================================
# SimpleCommKit iOS Toolchain
# ============================================================================
# 基于 leetal/ios-cmake (https://github.com/leetal/ios-cmake) 精简定制
# 仅支持 iOS / iOS Simulator 平台，预配置 SimpleCommKit 模块选项
#
# 用法:
#   # 真机 (arm64)
#   cmake -B build_ios -DCMAKE_TOOLCHAIN_FILE=cmake/ios.toolchain.cmake -DPLATFORM=OS64
#
#   # 模拟器 (Apple Silicon Mac 上推荐)
#   cmake -B build_sim -DCMAKE_TOOLCHAIN_FILE=cmake/ios.toolchain.cmake -DPLATFORM=SIMULATORARM64
#
#   # 模拟器 (Intel Mac)
#   cmake -B build_sim -DCMAKE_TOOLCHAIN_FILE=cmake/ios.toolchain.cmake -DPLATFORM=SIMULATOR64
#
#   # 构建
#   cmake --build build_ios
#
#   # FAT 静态库 (真机 + 模拟器二合一，需 -G Xcode)
#   cmake -B build_fat -G Xcode -DCMAKE_TOOLCHAIN_FILE=cmake/ios.toolchain.cmake -DPLATFORM=OS64COMBINED
#   cmake --build build_fat --config Release
#   cmake --install build_fat --config Release
#
# 可选参数:
#   PLATFORM            - 目标平台 (默认: OS64)
#     OS64              = arm64 真机
#     SIMULATORARM64    = arm64 模拟器 (Apple Silicon Mac)
#     SIMULATOR64       = x86_64 模拟器 (Intel Mac / Rosetta)
#     OS64COMBINED      = arm64 + x86_64 FAT 静态库 (需要 -G Xcode + cmake --install)
#   DEPLOYMENT_TARGET   - 最低 iOS 版本 (默认: 15.0)
#   ENABLE_BITCODE      - 是否启用 bitcode (默认: OFF, Xcode 14+ 已废弃)
#   ENABLE_ARC          - 是否启用 ARC (默认: ON)
#   ENABLE_VISIBILITY   - 是否隐藏符号 (默认: OFF, 即默认隐藏)
# ============================================================================

cmake_minimum_required(VERSION 3.19)

# ---------------------------------------------------------------------------
# 防止 CMake 重复加载 toolchain (仅第一次生效)
# ---------------------------------------------------------------------------
if(DEFINED ENV{_SCK_IOS_TOOLCHAIN_HAS_RUN})
  return()
endif()
set(ENV{_SCK_IOS_TOOLCHAIN_HAS_RUN} true)

# ---------------------------------------------------------------------------
# 支持的平台列表 (仅 iOS 相关)
# ---------------------------------------------------------------------------
list(APPEND _sck_supported_platforms
  "OS64" "SIMULATOR64" "SIMULATORARM64" "OS64COMBINED"
)

# 检查 CMake 版本是否支持 FAT combined 构建
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.14")
  set(SCK_MODERN_CMAKE YES)
endif()

# ---------------------------------------------------------------------------
# 查找 xcodebuild
# ---------------------------------------------------------------------------
find_program(XCODEBUILD_EXECUTABLE xcodebuild)
if(NOT XCODEBUILD_EXECUTABLE)
  message(FATAL_ERROR "xcodebuild not found. 请安装 Xcode 或 Command Line Tools.")
endif()

# 获取 Xcode 版本
execute_process(COMMAND ${XCODEBUILD_EXECUTABLE} -version
  OUTPUT_VARIABLE _xcode_version_str
  ERROR_QUIET
  OUTPUT_STRIP_TRAILING_WHITESPACE)
string(REGEX MATCH "Xcode [0-9\\.]+" _xcode_version_str "${_xcode_version_str}")
string(REGEX REPLACE "Xcode ([0-9\\.]+)" "\\1" XCODE_VERSION "${_xcode_version_str}")
message(STATUS "[SimpleCommKit iOS] Xcode 版本: ${XCODE_VERSION}")

# ---------------------------------------------------------------------------
# 平台参数处理
# ---------------------------------------------------------------------------
if(NOT DEFINED PLATFORM)
  set(PLATFORM "OS64" CACHE STRING "目标平台: OS64 | SIMULATORARM64 | SIMULATOR64 | OS64COMBINED")
  message(STATUS "[SimpleCommKit iOS] PLATFORM 未指定，使用默认值: OS64 (arm64 真机)")
endif()

list(FIND _sck_supported_platforms "${PLATFORM}" _plat_idx)
if(_plat_idx EQUAL -1)
  string(REPLACE ";" "\n  * " _plat_list "${_sck_supported_platforms}")
  message(FATAL_ERROR "不支持的 PLATFORM: ${PLATFORM}\n"
    "支持的平台:\n  * ${_plat_list}")
endif()

if(PLATFORM MATCHES ".*COMBINED" AND NOT CMAKE_GENERATOR MATCHES "Xcode")
  message(FATAL_ERROR "PLATFORM=${PLATFORM} 需要使用 Xcode generator: 添加 -G Xcode")
endif()

# ---------------------------------------------------------------------------
# Deployment Target
# ---------------------------------------------------------------------------
if(NOT DEFINED DEPLOYMENT_TARGET)
  set(DEPLOYMENT_TARGET "15.0" CACHE STRING "最低 iOS 部署版本")
  message(STATUS "[SimpleCommKit iOS] DEPLOYMENT_TARGET 未指定，使用默认值: 15.0")
endif()

# ---------------------------------------------------------------------------
# 根据 PLATFORM 确定 SDK / 架构 / target triple
# ---------------------------------------------------------------------------
if(PLATFORM STREQUAL "OS64")
  set(SDK_NAME iphoneos)
  set(ARCHS arm64)
  set(APPLE_TARGET_TRIPLE aarch64-apple-ios${DEPLOYMENT_TARGET})
elseif(PLATFORM STREQUAL "SIMULATOR64")
  set(SDK_NAME iphonesimulator)
  set(ARCHS x86_64)
  set(APPLE_TARGET_TRIPLE x86_64-apple-ios${DEPLOYMENT_TARGET}-simulator)
elseif(PLATFORM STREQUAL "SIMULATORARM64")
  set(SDK_NAME iphonesimulator)
  set(ARCHS arm64)
  set(APPLE_TARGET_TRIPLE aarch64-apple-ios${DEPLOYMENT_TARGET}-simulator)
elseif(PLATFORM STREQUAL "OS64COMBINED")
  set(SDK_NAME iphoneos)
  if(SCK_MODERN_CMAKE)
    set(ARCHS arm64 x86_64)
    set(APPLE_TARGET_TRIPLE aarch64-x86_64-apple-ios${DEPLOYMENT_TARGET})
    # Xcode 多架构配置
    set(CMAKE_XCODE_ATTRIBUTE_ARCHS[sdk=iphoneos*] "arm64")
    set(CMAKE_XCODE_ATTRIBUTE_ARCHS[sdk=iphonesimulator*] "x86_64")
    set(CMAKE_XCODE_ATTRIBUTE_VALID_ARCHS[sdk=iphoneos*] "arm64")
    set(CMAKE_XCODE_ATTRIBUTE_VALID_ARCHS[sdk=iphonesimulator*] "x86_64")
    set(CMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH "NO")
    set(CMAKE_IOS_INSTALL_COMBINED YES)
  else()
    message(FATAL_ERROR "PLATFORM=OS64COMBINED 需要 CMake 3.14+")
  endif()
endif()

# ---------------------------------------------------------------------------
# 查找 iOS SDK 路径
# ---------------------------------------------------------------------------
execute_process(COMMAND ${XCODEBUILD_EXECUTABLE} -version -sdk ${SDK_NAME} Path
  OUTPUT_VARIABLE CMAKE_OSX_SYSROOT
  ERROR_QUIET
  OUTPUT_STRIP_TRAILING_WHITESPACE)

if(NOT CMAKE_OSX_SYSROOT)
  message(FATAL_ERROR "找不到 ${SDK_NAME} SDK. 请确认 Xcode 已正确安装.")
endif()

# 获取 SDK 版本号
execute_process(COMMAND ${XCODEBUILD_EXECUTABLE} -sdk ${CMAKE_OSX_SYSROOT} -version SDKVersion
  OUTPUT_VARIABLE SDK_VERSION
  ERROR_QUIET
  OUTPUT_STRIP_TRAILING_WHITESPACE)

# ---------------------------------------------------------------------------
# 查找编译器 & 工具链
# ---------------------------------------------------------------------------
execute_process(COMMAND xcrun -sdk ${CMAKE_OSX_SYSROOT} -find clang
  OUTPUT_VARIABLE CMAKE_C_COMPILER
  ERROR_QUIET
  OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(COMMAND xcrun -sdk ${CMAKE_OSX_SYSROOT} -find clang++
  OUTPUT_VARIABLE CMAKE_CXX_COMPILER
  ERROR_QUIET
  OUTPUT_STRIP_TRAILING_WHITESPACE)

set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})

# 使用 Apple libtool 代替 ar/ranlib (Xcode 7+ 必须)
execute_process(COMMAND xcrun -sdk ${CMAKE_OSX_SYSROOT} -find libtool
  OUTPUT_VARIABLE BUILD_LIBTOOL
  ERROR_QUIET
  OUTPUT_STRIP_TRAILING_WHITESPACE)

get_property(_languages GLOBAL PROPERTY ENABLED_LANGUAGES)
foreach(_lang ${_languages})
  set(CMAKE_${_lang}_CREATE_STATIC_LIBRARY
    "${BUILD_LIBTOOL} -static -o <TARGET> <LINK_FLAGS> <OBJECTS> " CACHE INTERNAL "")
endforeach()

# ---------------------------------------------------------------------------
# 系统基础设置
# ---------------------------------------------------------------------------
set(CMAKE_SYSTEM_NAME iOS)
set(CMAKE_SYSTEM_VERSION ${SDK_VERSION} CACHE INTERNAL "")
set(CMAKE_OSX_ARCHITECTURES ${ARCHS} CACHE INTERNAL "")
set(CMAKE_OSX_DEPLOYMENT_TARGET ${DEPLOYMENT_TARGET} CACHE STRING "" FORCE)

set(UNIX ON CACHE BOOL "")
set(APPLE ON CACHE BOOL "")
set(IOS ON CACHE BOOL "")

set(CMAKE_C_COMPILER_TARGET ${APPLE_TARGET_TRIPLE})
set(CMAKE_CXX_COMPILER_TARGET ${APPLE_TARGET_TRIPLE})
set(CMAKE_ASM_COMPILER_TARGET ${APPLE_TARGET_TRIPLE})

# 交叉编译时 try_compile 编译为静态库 (不链接)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# ---------------------------------------------------------------------------
# Bitcode (Xcode 14+ 已废弃，默认关闭)
# ---------------------------------------------------------------------------
if(NOT DEFINED ENABLE_BITCODE)
  set(ENABLE_BITCODE OFF CACHE BOOL "启用 bitcode (Xcode 14+ 已废弃)")
endif()
if(ENABLE_BITCODE)
  set(BITCODE_FLAG "-fembed-bitcode")
  set(CMAKE_XCODE_ATTRIBUTE_ENABLE_BITCODE "YES")
else()
  set(BITCODE_FLAG "")
  set(CMAKE_XCODE_ATTRIBUTE_ENABLE_BITCODE "NO")
endif()

# ---------------------------------------------------------------------------
# ARC (默认开启)
# ---------------------------------------------------------------------------
if(NOT DEFINED ENABLE_ARC)
  set(ENABLE_ARC ON CACHE BOOL "启用 Automatic Reference Counting")
endif()
if(ENABLE_ARC)
  set(OBJC_ARC_FLAG "-fobjc-arc")
  set(CMAKE_XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC "YES")
else()
  set(OBJC_ARC_FLAG "-fno-objc-arc")
  set(CMAKE_XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC "NO")
endif()

# ---------------------------------------------------------------------------
# 符号可见性 (默认隐藏)
# ---------------------------------------------------------------------------
if(NOT DEFINED ENABLE_VISIBILITY)
  set(ENABLE_VISIBILITY OFF CACHE BOOL "启用符号可见性 (默认隐藏)")
endif()
if(ENABLE_VISIBILITY)
  foreach(_lang ${_languages})
    set(CMAKE_${_lang}_VISIBILITY_PRESET "default" CACHE INTERNAL "")
  endforeach()
  set(VISIBILITY_FLAG "-fvisibility=default")
else()
  foreach(_lang ${_languages})
    set(CMAKE_${_lang}_VISIBILITY_PRESET "hidden" CACHE INTERNAL "")
  endforeach()
  set(VISIBILITY_FLAG "-fvisibility=hidden -fvisibility-inlines-hidden")
  set(CMAKE_XCODE_ATTRIBUTE_GCC_SYMBOLS_PRIVATE_EXTERN "YES")
endif()

# ---------------------------------------------------------------------------
# 指针大小 / 处理器架构
# ---------------------------------------------------------------------------
if(ARCHS MATCHES "((^|;| )(arm64|arm64e|x86_64))+")
  set(CMAKE_C_SIZEOF_DATA_PTR 8)
  set(CMAKE_CXX_SIZEOF_DATA_PTR 8)
  if(ARCHS MATCHES "arm64")
    set(CMAKE_SYSTEM_PROCESSOR "aarch64")
  else()
    set(CMAKE_SYSTEM_PROCESSOR "x86_64")
  endif()
endif()

# ---------------------------------------------------------------------------
# 编译 / 链接标志
# ---------------------------------------------------------------------------
if(CMAKE_GENERATOR MATCHES "Xcode")
  message(STATUS "[SimpleCommKit iOS] 使用 Xcode generator，由 Xcode 管理编译标志")
  set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")
  set(CMAKE_XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "${DEPLOYMENT_TARGET}")
  if(NOT PLATFORM MATCHES ".*COMBINED")
    set(CMAKE_XCODE_ATTRIBUTE_ARCHS[sdk=${SDK_NAME}*] "${ARCHS}")
    set(CMAKE_XCODE_ATTRIBUTE_VALID_ARCHS[sdk=${SDK_NAME}*] "${ARCHS}")
  endif()
else()
  # Makefile / Ninja generator: 手动设置标志
  set(CMAKE_C_FLAGS "${BITCODE_FLAG} ${VISIBILITY_FLAG} ${CMAKE_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${BITCODE_FLAG} ${VISIBILITY_FLAG} ${CMAKE_CXX_FLAGS}")

  set(CMAKE_C_FLAGS_DEBUG   "-O0 -g ${CMAKE_C_FLAGS_DEBUG}")
  set(CMAKE_C_FLAGS_RELEASE "-DNDEBUG -O3 ${CMAKE_C_FLAGS_RELEASE}")
  set(CMAKE_CXX_FLAGS_DEBUG   "-O0 -g ${CMAKE_CXX_FLAGS_DEBUG}")
  set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -O3 ${CMAKE_CXX_FLAGS_RELEASE}")

  set(CMAKE_OBJC_FLAGS "${BITCODE_FLAG} ${VISIBILITY_FLAG} ${OBJC_ARC_FLAG} ${CMAKE_OBJC_FLAGS}")
  set(CMAKE_OBJCXX_FLAGS "${BITCODE_FLAG} ${VISIBILITY_FLAG} ${OBJC_ARC_FLAG} ${CMAKE_OBJCXX_FLAGS}")

  set(CMAKE_C_LINK_FLAGS "-Wl,-search_paths_first ${CMAKE_C_LINK_FLAGS}")
  set(CMAKE_CXX_LINK_FLAGS "-Wl,-search_paths_first ${CMAKE_CXX_LINK_FLAGS}")
endif()

# ---------------------------------------------------------------------------
# 动态库设置
# ---------------------------------------------------------------------------
set(CMAKE_PLATFORM_HAS_INSTALLNAME 1)
set(CMAKE_SHARED_LIBRARY_PREFIX "lib")
set(CMAKE_SHARED_LIBRARY_SUFFIX ".dylib")
set(CMAKE_SHARED_MODULE_PREFIX "lib")
set(CMAKE_SHARED_MODULE_SUFFIX ".so")
set(CMAKE_SHARED_LINKER_FLAGS "-rpath @executable_path/Frameworks -rpath @loader_path/Frameworks")
set(CMAKE_SHARED_LIBRARY_CREATE_C_FLAGS "-dynamiclib -Wl,-headerpad_max_install_names")
set(CMAKE_SHARED_MODULE_CREATE_C_FLAGS "-bundle -Wl,-headerpad_max_install_names")
set(CMAKE_FIND_LIBRARY_SUFFIXES ".tbd" ".dylib" ".so" ".a")

# ---------------------------------------------------------------------------
# Framework / 查找路径
# ---------------------------------------------------------------------------
set(CMAKE_FIND_ROOT_PATH "${CMAKE_OSX_SYSROOT}" CACHE INTERNAL "")
set(CMAKE_IGNORE_PATH "/System/Library/Frameworks;/usr/local/lib" CACHE INTERNAL "")
set(CMAKE_FIND_FRAMEWORK FIRST)
set(CMAKE_FRAMEWORK_PATH
  ${CMAKE_OSX_SYSROOT}/System/Library/Frameworks
  CACHE INTERNAL "")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BOTH CACHE INTERNAL "")
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH CACHE INTERNAL "")
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH CACHE INTERNAL "")
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH CACHE INTERNAL "")

# Code Signing 关闭 (静态库不需要签名)
set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO")

# ---------------------------------------------------------------------------
# SimpleCommKit 专属: 模块选项预设
# ---------------------------------------------------------------------------
# iOS 上自动禁用的模块 (硬件不支持)
set(ENABLE_SIMPLECOMMKIT_SERIALPORT OFF CACHE BOOL "串口 (iOS 不支持)" FORCE)
set(ENABLE_SIMPLECOMMKIT_HID        OFF CACHE BOOL "HID (iOS 不支持)" FORCE)
set(ENABLE_SIMPLECOMMKIT_USB        OFF CACHE BOOL "USB (iOS 不支持)" FORCE)

# 默认开启的网络模块
set(ENABLE_SIMPLECOMMKIT_TCP        ON  CACHE BOOL "TCP 通信" FORCE)
set(ENABLE_SIMPLECOMMKIT_UDP        ON  CACHE BOOL "UDP 通信" FORCE)
set(ENABLE_SIMPLECOMMKIT_WEBSOCKET  ON  CACHE BOOL "WebSocket 通信" FORCE)

# 可选模块
set(ENABLE_SIMPLECOMMKIT_BLE        OFF CACHE BOOL "BLE 蓝牙 (iOS 需额外集成 CoreBluetooth)")
set(ENABLE_SIMPLECOMMKIT_MQTTCLIENT OFF CACHE BOOL "MQTT 客户端")

# 关闭非 iOS 相关
set(SIMPLECOMMKIT_EXAMPLES           OFF CACHE BOOL "示例" FORCE)
set(ENABLE_SIMPLECOMMKITPYBIND       OFF CACHE BOOL "Python 绑定" FORCE)
set(ENABLE_SIMPLECOMMKITAI_FASTMCPP  OFF CACHE BOOL "FastMCPP" FORCE)

# ---------------------------------------------------------------------------
# 辅助宏
# ---------------------------------------------------------------------------

# 设置 Xcode 属性的便捷宏
macro(set_xcode_property TARGET XCODE_PROPERTY XCODE_VALUE XCODE_RELVERSION)
  set(_relver "${XCODE_RELVERSION}")
  if(_relver STREQUAL "All")
    set_property(TARGET ${TARGET} PROPERTY
      XCODE_ATTRIBUTE_${XCODE_PROPERTY} "${XCODE_VALUE}")
  else()
    set_property(TARGET ${TARGET} PROPERTY
      XCODE_ATTRIBUTE_${XCODE_PROPERTY}[variant=${_relver}] "${XCODE_VALUE}")
  endif()
endmacro()

# 在宿主机上查找包的便捷宏 (交叉编译时有用)
macro(find_host_package)
  set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
  set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY NEVER)
  set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE NEVER)
  set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE NEVER)
  set(_toolchain_ios ${IOS})
  set(IOS OFF)
  find_package(${ARGN})
  set(IOS ${_toolchain_ios})
  set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BOTH)
  set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
  set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
  set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
endmacro()

# ---------------------------------------------------------------------------
# 配置摘要
# ---------------------------------------------------------------------------
message(STATUS "=============================================================================")
message(STATUS "[SimpleCommKit iOS Toolchain] 配置摘要")
message(STATUS "  平台 (PLATFORM):          ${PLATFORM}")
message(STATUS "  架构 (ARCHS):             ${ARCHS}")
message(STATUS "  最低部署版本:              iOS ${DEPLOYMENT_TARGET}")
message(STATUS "  SDK 版本:                 ${SDK_VERSION}")
message(STATUS "  SDK 路径:                 ${CMAKE_OSX_SYSROOT}")
message(STATUS "  C 编译器:                 ${CMAKE_C_COMPILER}")
message(STATUS "  C++ 编译器:               ${CMAKE_CXX_COMPILER}")
message(STATUS "  Target Triple:            ${APPLE_TARGET_TRIPLE}")
message(STATUS "  Bitcode:                  ${ENABLE_BITCODE}")
message(STATUS "  ARC:                      ${ENABLE_ARC}")
message(STATUS "  符号可见性:               ${ENABLE_VISIBILITY}")
message(STATUS "  CMake Generator:          ${CMAKE_GENERATOR}")
message(STATUS "")
message(STATUS "  SimpleCommKit 模块:")
message(STATUS "    TCP:                    ${ENABLE_SIMPLECOMMKIT_TCP}")
message(STATUS "    UDP:                    ${ENABLE_SIMPLECOMMKIT_UDP}")
message(STATUS "    WebSocket:              ${ENABLE_SIMPLECOMMKIT_WEBSOCKET}")
message(STATUS "    BLE:                    ${ENABLE_SIMPLECOMMKIT_BLE}")
message(STATUS "    MQTT:                   ${ENABLE_SIMPLECOMMKIT_MQTTCLIENT}")
message(STATUS "    SerialPort/HID/USB:     OFF (iOS 不支持)")
message(STATUS "=============================================================================")

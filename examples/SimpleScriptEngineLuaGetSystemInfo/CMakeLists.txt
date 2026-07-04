# ============================================================================
# SimpleScriptEngine — LuaJIT FFI Windows System Info Example
# ============================================================================

project(SimpleScriptEngineLuaGetSystemInfo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(SimpleScriptEngineLuaGetSystemInfo main.cpp)

# 链接核心库
target_link_libraries(SimpleScriptEngineLuaGetSystemInfo PRIVATE SimpleScriptEngine)

# 将引擎启用宏传递给示例代码（用于条件编译）
if(SIMPLESCRIPTENGINE_ENABLE_LUAJIT)
    target_compile_definitions(SimpleScriptEngineLuaGetSystemInfo PRIVATE SIMPLESCRIPTENGINE_ENABLE_LUAJIT)
endif()

# 将 example 输出到统一目录
set_target_properties(SimpleScriptEngineLuaGetSystemInfo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# 运行时依赖：将 sysinfo-windows.lua 复制到 exe 同级目录
add_custom_command(TARGET SimpleScriptEngineLuaGetSystemInfo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/sysinfo-windows.lua"
        "$<TARGET_FILE_DIR:SimpleScriptEngineLuaGetSystemInfo>/sysinfo-windows.lua"
    COMMENT "Copying sysinfo-windows.lua to output directory"
)

# MSVC 设置
if(MSVC)
    target_compile_options(SimpleScriptEngineLuaGetSystemInfo PRIVATE /utf-8 /bigobj)
endif()

# ============================================================================
# SimpleScriptEngine — Example
# ============================================================================

project(SimpleScriptEngineExample LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(SimpleScriptEngineExample main.cpp)

# 链接核心库（include 路径自动从 PUBLIC 继承）
target_link_libraries(SimpleScriptEngineExample PRIVATE SimpleScriptEngine)

# 将引擎启用宏传递给示例代码（用于条件编译）
if(SIMPLESCRIPTENGINE_ENABLE_LUAJIT)
    target_compile_definitions(SimpleScriptEngineExample PRIVATE SIMPLESCRIPTENGINE_ENABLE_LUAJIT)
endif()
if(SIMPLESCRIPTENGINE_ENABLE_QUICKJS)
    target_compile_definitions(SimpleScriptEngineExample PRIVATE SIMPLESCRIPTENGINE_ENABLE_QUICKJS)
endif()
if(SIMPLESCRIPTENGINE_ENABLE_CHAISCRIPT)
    target_compile_definitions(SimpleScriptEngineExample PRIVATE SIMPLESCRIPTENGINE_ENABLE_CHAISCRIPT)
endif()
if(SIMPLESCRIPTENGINE_ENABLE_ANGELSCRIPT)
    target_compile_definitions(SimpleScriptEngineExample PRIVATE SIMPLESCRIPTENGINE_ENABLE_ANGELSCRIPT)
endif()

# 将 example 输出到统一目录
set_target_properties(SimpleScriptEngineExample PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# MSVC 设置
if(MSVC)
    target_compile_options(SimpleScriptEngineExample PRIVATE /utf-8 /bigobj)
endif()

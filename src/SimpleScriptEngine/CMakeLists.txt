# ============================================================================
# SimpleScriptEngine — 核心库
# ============================================================================
# 统一脚本引擎抽象层，包含：
#   - SimpleScriptValue    : 跨引擎统一值类型 (header-only)
#   - SimpleScriptEngine   : 引擎抽象接口 (header-only)
#   - SimpleScriptManager  : 引擎管理器 / 智能路由
#   - Engines/             : 4 个脚本引擎适配器 (LuaJIT / QuickJS / ChaiScript / AngelScript)
# ============================================================================

project(SimpleScriptEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# 库类型
# ============================================================================

if(SIMPLESCRIPTENGINE_BUILD_SHARED)
    set(LIB_TYPE SHARED)
else()
    set(LIB_TYPE STATIC)
endif()

# ============================================================================
# 核心源文件
# ============================================================================

set(CORE_HEADERS
    SimpleScriptEngine.h
    SimpleScriptValue.h
    SimpleScriptManager.h
)

set(CORE_SOURCES
    SimpleScriptManager.cpp
)

# ============================================================================
# 引擎源文件（按选项条件添加）
# ============================================================================

set(ENGINE_SOURCES "")

# ---- LuaJIT ----
if(SIMPLESCRIPTENGINE_ENABLE_LUAJIT)
    list(APPEND ENGINE_SOURCES
        Engines/LuaJITEngine.h
        Engines/LuaJITEngine.cpp
    )
endif()

# ---- QuickJS ----
if(SIMPLESCRIPTENGINE_ENABLE_QUICKJS)
    list(APPEND ENGINE_SOURCES
        Engines/QuickJSEngine.h
        Engines/QuickJSEngine.cpp
    )
endif()

# ---- ChaiScript ----
if(SIMPLESCRIPTENGINE_ENABLE_CHAISCRIPT)
    list(APPEND ENGINE_SOURCES
        Engines/ChaiScriptEngine.h
        Engines/ChaiScriptEngine.cpp
    )
endif()

# ---- AngelScript ----
if(SIMPLESCRIPTENGINE_ENABLE_ANGELSCRIPT)
    list(APPEND ENGINE_SOURCES
        Engines/AngelScriptEngine.h
        Engines/AngelScriptEngine.cpp
    )
endif()

# ============================================================================
# 创建目标
# ============================================================================

add_library(SimpleScriptEngine ${LIB_TYPE}
    ${CORE_HEADERS}
    ${CORE_SOURCES}
    ${ENGINE_SOURCES}
)

# PUBLIC: 下游使用时需要的 include 路径
target_include_directories(SimpleScriptEngine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Engines>
        $<INSTALL_INTERFACE:include/SimpleScriptEngine>
)

# ============================================================================
# 编译定义 — 控制各引擎 .cpp 中的 #ifdef 分支
# ============================================================================

# ChaiScript 是 header-only 模板库，实例化产生大量段，需 /bigobj
if(MSVC)
    target_compile_options(SimpleScriptEngine PRIVATE /bigobj)
endif()

if(SIMPLESCRIPTENGINE_ENABLE_LUAJIT)
    target_compile_definitions(SimpleScriptEngine PRIVATE SIMPLESCRIPTENGINE_ENABLE_LUAJIT)
endif()

if(SIMPLESCRIPTENGINE_ENABLE_QUICKJS)
    target_compile_definitions(SimpleScriptEngine PRIVATE SIMPLESCRIPTENGINE_ENABLE_QUICKJS)
endif()

if(SIMPLESCRIPTENGINE_ENABLE_CHAISCRIPT)
    target_compile_definitions(SimpleScriptEngine PRIVATE SIMPLESCRIPTENGINE_ENABLE_CHAISCRIPT)
endif()

if(SIMPLESCRIPTENGINE_ENABLE_ANGELSCRIPT)
    target_compile_definitions(SimpleScriptEngine PRIVATE SIMPLESCRIPTENGINE_ENABLE_ANGELSCRIPT)
endif()

# ============================================================================
# 链接 3rdparty 引擎目标 & 设置对应 include 路径
# ============================================================================

# ---- LuaJIT (luajit-rocks) ----
if(SIMPLESCRIPTENGINE_ENABLE_LUAJIT)
    # luajit-rocks 构建产物，目标名可能为 libluajit / luajit-static
    if(TARGET luajit-static)
        target_link_libraries(SimpleScriptEngine PRIVATE luajit-static)
    elseif(TARGET libluajit)
        target_link_libraries(SimpleScriptEngine PRIVATE libluajit)
    elseif(TARGET luajit)
        target_link_libraries(SimpleScriptEngine PRIVATE luajit)
    else()
        message(WARNING "LuaJIT target not found — ensure luajit-rocks is added before this subdirectory")
    endif()
    # luajit-rocks 使用 include_directories() 而非 target_include_directories()，
    # include 路径不会自动传播到兄弟目录，需要手动设置
    target_include_directories(SimpleScriptEngine PRIVATE
        ${3RDPARTY_PATH}/luajit-rocks/luajit-2.1/src)
endif()

# ---- QuickJS (quickjs-ng) ----
if(SIMPLESCRIPTENGINE_ENABLE_QUICKJS)
    if(TARGET quickjs)
        target_link_libraries(SimpleScriptEngine PRIVATE quickjs)
    elseif(TARGET qjs)
        target_link_libraries(SimpleScriptEngine PRIVATE qjs)
    else()
        message(WARNING "QuickJS target not found — ensure quickjs subdirectory is added")
    endif()
    target_include_directories(SimpleScriptEngine PRIVATE ${3RDPARTY_PATH}/quickjs)
endif()

# ---- ChaiScript (header-only) ----
if(SIMPLESCRIPTENGINE_ENABLE_CHAISCRIPT)
    if(TARGET chaiscript)
        target_link_libraries(SimpleScriptEngine PRIVATE chaiscript)
    elseif(TARGET ChaiScript)
        target_link_libraries(SimpleScriptEngine PRIVATE ChaiScript)
    else()
        # ChaiScript 是 header-only，只需设置 include 路径
    endif()
    target_include_directories(SimpleScriptEngine PRIVATE ${3RDPARTY_PATH}/ChaiScript/include)
endif()

# ---- AngelScript ----
if(SIMPLESCRIPTENGINE_ENABLE_ANGELSCRIPT)
    if(TARGET angelscript)
        target_link_libraries(SimpleScriptEngine PRIVATE angelscript)
    elseif(TARGET AngelScript)
        target_link_libraries(SimpleScriptEngine PRIVATE AngelScript)
    else()
        message(WARNING "AngelScript target not found — ensure angelscript subdirectory is added")
    endif()
    target_include_directories(SimpleScriptEngine PRIVATE
        ${3RDPARTY_PATH}/angelscript/sdk/angelscript/include
        ${3RDPARTY_PATH}/angelscript/sdk/add_on/scriptstdstring
        ${3RDPARTY_PATH}/angelscript/sdk/add_on/scriptarray
    )
    # 编译 AngelScript add-on 源文件（RegisterStdString / RegisterScriptArray）
    target_sources(SimpleScriptEngine PRIVATE
        ${3RDPARTY_PATH}/angelscript/sdk/add_on/scriptstdstring/scriptstdstring.cpp
        ${3RDPARTY_PATH}/angelscript/sdk/add_on/scriptarray/scriptarray.cpp
    )
endif()

# ============================================================================
# 安装规则
# ============================================================================

install(TARGETS SimpleScriptEngine
    EXPORT SimpleScriptEngineTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES
    SimpleScriptEngine.h
    SimpleScriptValue.h
    SimpleScriptManager.h
    DESTINATION include/SimpleScriptEngine
)

install(FILES
    Engines/LuaJITEngine.h
    Engines/QuickJSEngine.h
    Engines/ChaiScriptEngine.h
    Engines/AngelScriptEngine.h
    DESTINATION include/SimpleScriptEngine/Engines
)

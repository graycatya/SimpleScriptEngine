# ============================================================================
# SimpleScriptEngine -- Core Library
# ============================================================================

project(SimpleScriptEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(SIMPLESCRIPTENGINE_BUILD_SHARED)
    set(LIB_TYPE SHARED)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
else()
    set(LIB_TYPE STATIC)
endif()

# ---- core ----

set(CORE_HEADERS
    SimpleScriptEngine.h
    SimpleScriptValue.h
    SimpleScriptManager.h
)

set(CORE_SOURCES
    SimpleScriptManager.cpp
)

# ---- engines ----

set(ENGINE_SOURCES "")

if(SIMPLESCRIPTENGINE_ENABLE_LUAJIT)
    list(APPEND ENGINE_SOURCES
        Engines/LuaJITEngine.h
        Engines/LuaJITEngine.cpp
    )
endif()

if(SIMPLESCRIPTENGINE_ENABLE_QUICKJS)
    list(APPEND ENGINE_SOURCES
        Engines/QuickJSEngine.h
        Engines/QuickJSEngine.cpp
    )
endif()

if(SIMPLESCRIPTENGINE_ENABLE_CHAISCRIPT)
    list(APPEND ENGINE_SOURCES
        Engines/ChaiScriptEngine.h
        Engines/ChaiScriptEngine.cpp
    )
endif()

if(SIMPLESCRIPTENGINE_ENABLE_ANGELSCRIPT)
    list(APPEND ENGINE_SOURCES
        Engines/AngelScriptEngine.h
        Engines/AngelScriptEngine.cpp
    )
endif()

# ---- crypto (SSEP pak protection) ----

set(CRYPTO_SOURCES "")

if(SIMPLESCRIPTENGINE_ENABLE_PAK_PROTECTION)
    list(APPEND CRYPTO_SOURCES
        Crypto/SSEPakFormat.h
        Crypto/SSECrypto.h
        Crypto/SSECrypto.cpp
        Crypto/SSEKeyManager.h
        Crypto/SSEKeyManager.cpp
        Crypto/SSEAntiDebug.h
        Crypto/SSEAntiDebug.cpp
        Crypto/SSEPakReader.h
        Crypto/SSEPakReader.cpp
        Crypto/SSEPakLoader.h
        Crypto/SSEPakLoader.cpp
    )
endif()

# ---- target ----

add_library(SimpleScriptEngine ${LIB_TYPE}
    ${CORE_HEADERS}
    ${CORE_SOURCES}
    ${ENGINE_SOURCES}
    ${CRYPTO_SOURCES}
)

target_include_directories(SimpleScriptEngine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Engines>
        $<INSTALL_INTERFACE:include/SimpleScriptEngine>
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Crypto>
)

# ---- compile definitions ----

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

# ---- SSEP pak protection ----

if(SIMPLESCRIPTENGINE_ENABLE_PAK_PROTECTION)
    target_compile_definitions(SimpleScriptEngine PRIVATE SIMPLESCRIPTENGINE_ENABLE_PAK_PROTECTION)

    if(SIMPLESCRIPTENGINE_ANTI_DEBUG STREQUAL "HEAVY")
        target_compile_definitions(SimpleScriptEngine PRIVATE SIMPLESCRIPTENGINE_ANTI_DEBUG_HEAVY)
    elseif(SIMPLESCRIPTENGINE_ANTI_DEBUG STREQUAL "LIGHT")
        target_compile_definitions(SimpleScriptEngine PRIVATE SIMPLESCRIPTENGINE_ANTI_DEBUG_LIGHT)
    endif()

    target_link_libraries(SimpleScriptEngine PRIVATE OpenSSL::Crypto miniz)
endif()

# ---- 3rdparty engines ----

if(SIMPLESCRIPTENGINE_ENABLE_LUAJIT)
    if(TARGET luajit-static)
        target_link_libraries(SimpleScriptEngine PRIVATE luajit-static)
    elseif(TARGET libluajit)
        target_link_libraries(SimpleScriptEngine PRIVATE libluajit)
    elseif(TARGET luajit)
        target_link_libraries(SimpleScriptEngine PRIVATE luajit)
    else()
        message(WARNING "LuaJIT target not found")
    endif()
    target_include_directories(SimpleScriptEngine PRIVATE
        ${3RDPARTY_PATH}/luajit-rocks/luajit-2.1/src)
endif()

if(SIMPLESCRIPTENGINE_ENABLE_QUICKJS)
    if(TARGET quickjs)
        target_link_libraries(SimpleScriptEngine PRIVATE quickjs)
    elseif(TARGET qjs)
        target_link_libraries(SimpleScriptEngine PRIVATE qjs)
    else()
        message(WARNING "QuickJS target not found")
    endif()
    target_include_directories(SimpleScriptEngine PRIVATE ${3RDPARTY_PATH}/quickjs)
endif()

if(SIMPLESCRIPTENGINE_ENABLE_CHAISCRIPT)
    if(TARGET chaiscript)
        target_link_libraries(SimpleScriptEngine PRIVATE chaiscript)
    elseif(TARGET ChaiScript)
        target_link_libraries(SimpleScriptEngine PRIVATE ChaiScript)
    endif()
    target_include_directories(SimpleScriptEngine PRIVATE ${3RDPARTY_PATH}/ChaiScript/include)
endif()

if(SIMPLESCRIPTENGINE_ENABLE_ANGELSCRIPT)
    if(TARGET angelscript)
        target_link_libraries(SimpleScriptEngine PRIVATE angelscript)
    elseif(TARGET AngelScript)
        target_link_libraries(SimpleScriptEngine PRIVATE AngelScript)
    else()
        message(WARNING "AngelScript target not found")
    endif()
    target_include_directories(SimpleScriptEngine PRIVATE
        ${3RDPARTY_PATH}/angelscript/sdk/angelscript/include
        ${3RDPARTY_PATH}/angelscript/sdk/add_on/scriptstdstring
        ${3RDPARTY_PATH}/angelscript/sdk/add_on/scriptarray
    )
    target_sources(SimpleScriptEngine PRIVATE
        ${3RDPARTY_PATH}/angelscript/sdk/add_on/scriptstdstring/scriptstdstring.cpp
        ${3RDPARTY_PATH}/angelscript/sdk/add_on/scriptarray/scriptarray.cpp
    )
endif()

# ---- install ----

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

if(SIMPLESCRIPTENGINE_ENABLE_PAK_PROTECTION)
    install(FILES
        Crypto/SSEPakFormat.h
        Crypto/SSECrypto.h
        Crypto/SSEKeyManager.h
        Crypto/SSEAntiDebug.h
        Crypto/SSEPakReader.h
        Crypto/SSEPakLoader.h
        DESTINATION include/SimpleScriptEngine/Crypto
    )
endif()

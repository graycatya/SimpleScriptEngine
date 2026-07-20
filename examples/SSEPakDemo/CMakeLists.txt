project(SSEPakDemo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(SSEPakDemo main.cpp)

target_link_libraries(SSEPakDemo PRIVATE SimpleScriptEngine)

if(SIMPLESCRIPTENGINE_ENABLE_PAK_PROTECTION)
    target_link_libraries(SSEPakDemo PRIVATE OpenSSL::Crypto miniz)
endif()

if(SIMPLESCRIPTENGINE_ENABLE_LUAJIT)
    target_compile_definitions(SSEPakDemo PRIVATE SIMPLESCRIPTENGINE_ENABLE_LUAJIT)
endif()
if(SIMPLESCRIPTENGINE_ENABLE_QUICKJS)
    target_compile_definitions(SSEPakDemo PRIVATE SIMPLESCRIPTENGINE_ENABLE_QUICKJS)
endif()

set_target_properties(SSEPakDemo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(MSVC)
    target_compile_options(SSEPakDemo PRIVATE /utf-8 /bigobj)
endif()

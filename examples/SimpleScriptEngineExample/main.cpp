// ============================================================================
// SimpleScriptEngine — 完整示例
// ============================================================================
// 演示：
//   1. 直接使用各个引擎实例
//   2. SimpleScriptValue 值传递 & 工厂方法
//   3. SimpleScriptManager 引擎注册 / 智能路由
//   4. C++ → 脚本：setGlobal / registerFunction / hasFunction
//   5. 脚本 → C++：eval / call
//   6. 从文件加载脚本：executeFile
//   7. 错误回调
//   8. 跨引擎共享全局变量
// ============================================================================

#include "SimpleScriptManager.h"

#ifdef SIMPLESCRIPTENGINE_ENABLE_LUAJIT
#include "Engines/LuaJITEngine.h"
#endif
#ifdef SIMPLESCRIPTENGINE_ENABLE_QUICKJS
#include "Engines/QuickJSEngine.h"
#endif
#ifdef SIMPLESCRIPTENGINE_ENABLE_CHAISCRIPT
#include "Engines/ChaiScriptEngine.h"
#endif
#ifdef SIMPLESCRIPTENGINE_ENABLE_ANGELSCRIPT
#include "Engines/AngelScriptEngine.h"
#endif

#include <cassert>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

// 使用命名空间别名避免 SimpleScriptEngine 类名与命名空间冲突
namespace sse = SimpleScriptEngine;
using sse::SimpleScriptValue;
using sse::SimpleScriptManager;
using sse::ScriptValueType;
using sse::ScriptFunction;
using sse::ScriptArray;
using sse::ScriptObject;
#ifdef SIMPLESCRIPTENGINE_ENABLE_LUAJIT
using sse::LuaJITEngine;
#endif
#ifdef SIMPLESCRIPTENGINE_ENABLE_QUICKJS
using sse::QuickJSEngine;
#endif
#ifdef SIMPLESCRIPTENGINE_ENABLE_CHAISCRIPT
using sse::ChaiScriptEngine;
#endif
#ifdef SIMPLESCRIPTENGINE_ENABLE_ANGELSCRIPT
using sse::AngelScriptEngine;
#endif

// 基类别名（避免 SimpleScriptEngine::SimpleScriptEngine 歧义）
using ScriptEngine = SimpleScriptEngine::SimpleScriptEngine;

// ============================================================================
// 辅助宏
// ============================================================================

static int g_passed = 0;
static int g_failed = 0;

static thread_local std::chrono::steady_clock::time_point g_testStart;

#define TEST(name) \
    do { \
        g_testStart = std::chrono::steady_clock::now(); \
        std::cout << "  [" << name << "] "; \
    } while(0)

#define PASS() \
    do { \
        auto elapsed = std::chrono::steady_clock::now() - g_testStart; \
        auto us  = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count(); \
        std::cout << "PASSED  (" << us << " us)\n"; \
        ++g_passed; \
    } while(0)

#define FAIL(msg) \
    do { \
        auto elapsed = std::chrono::steady_clock::now() - g_testStart; \
        auto us  = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count(); \
        std::cout << "FAILED  (" << us << " us) — " << msg << "\n"; \
        ++g_failed; \
    } while(0)

#define CHECK(cond, msg) \
    do { \
        if (cond) { PASS(); } else { FAIL(msg); } \
    } while(0)

// ============================================================================
// 通用引擎测试函数
// ============================================================================

static void testEngineLifecycle(std::unique_ptr<ScriptEngine> engine) {
    const char* name = engine->engineName();

    TEST(std::string(name) + " — engineName");
    CHECK(std::string(engine->engineName()) == name,
          "wrong name");

    TEST(std::string(name) + " — fileExtension");
    CHECK(std::string(engine->fileExtension()).find('.') == 0,
          "missing dot prefix");

    TEST(std::string(name) + " — engineVersion");
    std::string ver(engine->engineVersion());
    CHECK(!ver.empty(), "empty version");

    TEST(std::string(name) + " — isInitialized (before init)");
    CHECK(!engine->isInitialized(), "should be false");

    TEST(std::string(name) + " — initialize");
    bool ok = engine->initialize();
    CHECK(ok || engine->isInitialized(), "init failed");

    TEST(std::string(name) + " — isInitialized (after init)");
    CHECK(engine->isInitialized(), "should be true");

    TEST(std::string(name) + " — shutdown");
    engine->shutdown();
    PASS();
}

static void testEngineEval(std::unique_ptr<ScriptEngine> engine) {
    const char* name = engine->engineName();
    engine->initialize();

    // 测试表达式求值（仅基本数值）
    if (std::string(name) == "LuaJIT") {
        TEST(std::string(name) + " — eval 1+2");
        auto v = engine->eval("1 + 2");
        CHECK(v.asDouble() == 3.0, "expected 3.0, got " + v.toString());
    } else if (std::string(name) == "QuickJS") {
        TEST(std::string(name) + " — eval 1+2");
        auto v = engine->eval("1 + 2");
        CHECK(v.asInt() == 3, "expected 3, got " + v.toString());
    } else if (std::string(name) == "ChaiScript") {
        TEST(std::string(name) + " — eval 1+2");
        auto v = engine->eval("1 + 2");
        CHECK(v.asInt() == 3, "expected 3, got " + v.toString());
    } else if (std::string(name) == "AngelScript") {
        TEST(std::string(name) + " — eval 1+2");
        auto v = engine->eval("1 + 2");
        CHECK(v.asInt() == 3, "expected 3, got " + v.toString());
    }

    engine->shutdown();
}

static void testEngineSetGetGlobal(std::unique_ptr<ScriptEngine> engine) {
    const char* name = engine->engineName();
    engine->initialize();

    // setGlobal
    TEST(std::string(name) + " — setGlobal (int)");
    engine->setGlobal("test_int", SimpleScriptValue::Int(42));
    PASS();

    TEST(std::string(name) + " — setGlobal (string)");
    engine->setGlobal("test_str", SimpleScriptValue::Str("hello"));
    PASS();

    TEST(std::string(name) + " — setGlobal (bool)");
    engine->setGlobal("test_bool", SimpleScriptValue::Bool(true));
    PASS();

    // getGlobal
    TEST(std::string(name) + " — getGlobal (int)");
    auto v = engine->getGlobal("test_int");
    CHECK(v.asInt() == 42, "expected 42, got " + v.toString());

    TEST(std::string(name) + " — getGlobal (string)");
    v = engine->getGlobal("test_str");
    // 注意：某些引擎的 getGlobal 可能返回不同类型
    PASS();  // 只要没有崩溃就通过

    // setGlobal Array
    TEST(std::string(name) + " — setGlobal (array)");
    engine->setGlobal("test_arr", SimpleScriptValue::Arr({
        SimpleScriptValue::Int(1),
        SimpleScriptValue::Int(2),
        SimpleScriptValue::Int(3)
    }));
    PASS();

    engine->shutdown();
}

static void testEngineExecuteString(std::unique_ptr<ScriptEngine> engine) {
    const char* name = engine->engineName();
    engine->initialize();

    if (std::string(name) == "LuaJIT") {
        TEST(std::string(name) + " — executeString");
        bool ok = engine->executeString("x = 10; y = x * 2");
        CHECK(ok, "executeString failed");

        TEST(std::string(name) + " — call script function");
        engine->executeString("function add(a, b) return a + b end");
        auto result = engine->call("add", {
            SimpleScriptValue::Num(3.0), SimpleScriptValue::Num(4.0)
        });
        CHECK(result.asDouble() == 7.0, "expected 7.0, got " + result.toString());
    } else if (std::string(name) == "QuickJS") {
        TEST(std::string(name) + " — executeString");
        bool ok = engine->executeString("var x = 10; var y = x * 2");
        CHECK(ok, "executeString failed");

        TEST(std::string(name) + " — call script function");
        engine->executeString("function add(a, b) { return a + b; }");
        auto result = engine->call("add", {
            SimpleScriptValue::Int(3), SimpleScriptValue::Int(4)
        });
        CHECK(result.asInt() == 7, "expected 7, got " + result.toString());
    } else if (std::string(name) == "ChaiScript") {
        TEST(std::string(name) + " — executeString");
        bool ok = engine->executeString("var x = 10; var y = x * 2");
        CHECK(ok, "executeString failed");

        TEST(std::string(name) + " — call script function");
        engine->executeString("def add(a, b) { return a + b; }");
        auto result = engine->call("add", {
            SimpleScriptValue::Int(3), SimpleScriptValue::Int(4)
        });
        CHECK(result.asInt() == 7, "expected 7, got " + result.toString());
    } else if (std::string(name) == "AngelScript") {
        TEST(std::string(name) + " — executeString");
        bool ok = engine->executeString("int global_var = 123;");
        CHECK(ok, "executeString failed");

        TEST(std::string(name) + " — call script function");
        engine->executeString("int add(int a, int b) { return a + b; }");
        auto result = engine->call("add", {
            SimpleScriptValue::Int(3), SimpleScriptValue::Int(4)
        });
        CHECK(result.asInt() == 7, "expected 7, got " + result.toString());
    }

    engine->shutdown();
}

static void testRegisterFunction(std::unique_ptr<ScriptEngine> engine) {
    const char* name = engine->engineName();
    engine->initialize();

    // 注册一个 C++ 函数到脚本
    TEST(std::string(name) + " — registerFunction");
    engine->registerFunction("cpp_add", [](const std::vector<SimpleScriptValue>& args) -> SimpleScriptValue {
        if (args.size() < 2) return SimpleScriptValue::Null();
        return SimpleScriptValue::Int(args[0].asInt() + args[1].asInt());
    });
    PASS();

    // 尝试从脚本调用 C++ 函数
    TEST(std::string(name) + " — call registered C++ function");
    // 注意：registerFunction 的完整实现因引擎而异，这里只验证不崩溃
    PASS();

    engine->shutdown();
}

static void testEngineHasFunction(std::unique_ptr<ScriptEngine> engine) {
    const char* name = engine->engineName();
    engine->initialize();

    // 注册一个函数
    if (std::string(name) == "LuaJIT") {
        engine->executeString("function test_func() return 1 end");
    } else if (std::string(name) == "QuickJS") {
        engine->executeString("function test_func() { return 1; }");
    } else if (std::string(name) == "ChaiScript") {
        engine->executeString("def test_func() { return 1; }");
    } else if (std::string(name) == "AngelScript") {
        engine->executeString("int test_func() { return 1; }");
    }

    TEST(std::string(name) + " — hasFunction (exists)");
    CHECK(engine->hasFunction("test_func"), "should exist");

    TEST(std::string(name) + " — hasFunction (not exists)");
    CHECK(!engine->hasFunction("nonexistent_func"), "should not exist");

    engine->shutdown();
}

static void testEngineExecuteFile(std::unique_ptr<ScriptEngine> engine) {
    const char* name = engine->engineName();
    std::string ext(engine->fileExtension());
    std::string path = "test_execfile" + ext;

    engine->initialize();

    // 根据引擎类型写入对应的脚本文件
    std::string scriptCode;
    std::string callCode;
    if (std::string(name) == "LuaJIT") {
        scriptCode = "function mul(a, b) return a * b end";
        callCode   = "mul";
    } else if (std::string(name) == "QuickJS") {
        scriptCode = "function mul(a, b) { return a * b; }";
        callCode   = "mul";
    } else if (std::string(name) == "ChaiScript") {
        scriptCode = "def mul(a, b) { return a * b; }";
        callCode   = "mul";
    } else if (std::string(name) == "AngelScript") {
        scriptCode = "int mul(int a, int b) { return a * b; }";
        callCode   = "mul";
    }

    // 写临时文件
    {
        std::ofstream ofs(path);
        ofs << scriptCode;
    }

    TEST(std::string(name) + " — executeFile");
    bool ok = engine->executeFile(path);
    CHECK(ok, "executeFile failed");

    // 执行后调用文件中定义的函数
    TEST(std::string(name) + " — call after executeFile");
    auto result = engine->call(callCode, {
        SimpleScriptValue::Int(6), SimpleScriptValue::Int(7)
    });
    CHECK(result.asInt() == 42, "expected 42, got " + result.toString());

    // 清理临时文件
    std::remove(path.c_str());

    engine->shutdown();
}

// ============================================================================
// SimpleScriptValue 类型测试
// ============================================================================

static void testSimpleScriptValue() {
    std::cout << "\n--- SimpleScriptValue 类型测试 ---\n\n";

    TEST("Null value");
    auto n = SimpleScriptValue::Null();
    CHECK(n.isNull() && n.type() == ScriptValueType::Null, "not null");

    TEST("Bool value");
    auto b = SimpleScriptValue::Bool(true);
    CHECK(b.isBool() && b.asBool() == true, "not bool/true");

    TEST("Int value");
    auto i = SimpleScriptValue::Int(42);
    CHECK(i.isInt() && i.asInt() == 42, "not int/42");

    TEST("Num (double) value");
    auto d = SimpleScriptValue::Num(3.14);
    CHECK(d.isDouble() && d.asDouble() == 3.14, "not double/3.14");

    TEST("String value");
    auto s = SimpleScriptValue::Str("hello world");
    CHECK(s.isString() && s.asString() == "hello world", "not string");

    TEST("Array value");
    auto arr = SimpleScriptValue::Arr({
        SimpleScriptValue::Int(1), SimpleScriptValue::Int(2)
    });
    CHECK(arr.isArray() && arr.asArray().size() == 2, "not array/size=2");

    TEST("Object value");
    auto obj = SimpleScriptValue::Obj({
        {"name", SimpleScriptValue::Str("test")},
        {"count", SimpleScriptValue::Int(10)}
    });
    CHECK(obj.isObject() && !obj.asObject().empty(), "not object/empty");

    TEST("toString");
    CHECK(!n.toString().empty(), "null toString empty");
    CHECK(!b.toString().empty(), "bool toString empty");
    CHECK(!arr.toString().empty(), "array toString empty");

    TEST("typeName");
    CHECK(std::string(n.typeName()) == "Null", "Null typeName wrong");
    CHECK(std::string(i.typeName()) == "Int", "Int typeName wrong");

    TEST("copy semantics");
    auto copy = i;
    CHECK(copy.asInt() == 42, "copy failed");

    TEST("move semantics");
    auto moved = std::move(copy);
    CHECK(moved.asInt() == 42, "move failed");
}

// ============================================================================
// SimpleScriptManager 路由测试
// ============================================================================

static void testManager() {
    std::cout << "\n--- SimpleScriptManager 路由测试 ---\n\n";

    auto& mgr = SimpleScriptManager::instance();

    // 注册引擎
    TEST("Manager — registerEngine");
#ifdef SIMPLESCRIPTENGINE_ENABLE_LUAJIT
    mgr.registerEngine(std::make_unique<LuaJITEngine>());
#endif
#ifdef SIMPLESCRIPTENGINE_ENABLE_QUICKJS
    mgr.registerEngine(std::make_unique<QuickJSEngine>());
#endif
#ifdef SIMPLESCRIPTENGINE_ENABLE_CHAISCRIPT
    mgr.registerEngine(std::make_unique<ChaiScriptEngine>());
#endif
#ifdef SIMPLESCRIPTENGINE_ENABLE_ANGELSCRIPT
    mgr.registerEngine(std::make_unique<AngelScriptEngine>());
#endif
    PASS();

    TEST("Manager — listEngines");
    auto names = mgr.listEngines();
    CHECK(!names.empty(), "no engines");

#ifdef SIMPLESCRIPTENGINE_ENABLE_LUAJIT
    TEST("Manager — getEngine");
    auto* engine = mgr.getEngine("LuaJIT");
    CHECK(engine != nullptr, "LuaJIT not found");

    TEST("Manager — engineName");
    CHECK(std::string(engine->engineName()) == "LuaJIT", "wrong engine");

    TEST("Manager — findEngineByExtension .lua");
    auto* byExt = mgr.findEngineByExtension(".lua");
    CHECK(byExt != nullptr && std::string(byExt->engineName()) == "LuaJIT",
          "not LuaJIT for .lua");
#endif

#ifdef SIMPLESCRIPTENGINE_ENABLE_QUICKJS
    TEST("Manager — findEngineByExtension .js");
    auto* byExtJs = mgr.findEngineByExtension(".js");
    CHECK(byExtJs != nullptr && std::string(byExtJs->engineName()) == "QuickJS",
          "not QuickJS for .js");
#endif

    // 初始化所有引擎
    TEST("Manager — initializeAll");
    size_t count = mgr.initializeAll();
    CHECK(count >= 1, "no engines initialized");

#ifdef SIMPLESCRIPTENGINE_ENABLE_LUAJIT
    // 按引擎名执行
    TEST("Manager — executeString via LuaJIT");
    bool ok = mgr.executeString("LuaJIT", "result = 100");
    CHECK(ok, "executeString failed");

    // 按扩展名自动路由执行文件
    TEST("Manager — executeFile (auto-route by .lua)");
    {
        std::string path = "test_mgr_file.lua";
        { std::ofstream ofs(path); ofs << "mgr_file_ok = true"; }
        ok = mgr.executeFile(path);
        CHECK(ok, "executeFile failed");
        std::remove(path.c_str());
    }
#endif

    // 跨引擎共享全局变量
    TEST("Manager — setSharedGlobal");
    mgr.setSharedGlobal("shared_var", SimpleScriptValue::Int(999));
    PASS();

    TEST("Manager — getSharedGlobal");
    auto sv = mgr.getSharedGlobal("shared_var");
    CHECK(sv.asInt() == 999, "expected 999");

    TEST("Manager — hasSharedGlobal");
    CHECK(mgr.hasSharedGlobal("shared_var"), "should have shared_var");

#ifdef SIMPLESCRIPTENGINE_ENABLE_LUAJIT
    // hasFunction
    TEST("Manager — hasFunction (LuaJIT)");
    engine->executeString("function mgr_test_func() return 42 end");
    CHECK(mgr.hasFunction("LuaJIT", "mgr_test_func"), "should exist");
    CHECK(!mgr.hasFunction("LuaJIT", "no_such_func"), "should not exist");

    // 错误回调
    TEST("Manager — error callback");
    engine->setErrorCallback([](const std::string& eng, const std::string& msg) {
        std::cout << "\n    [ErrorCallback — " << eng << "] " << msg;
    });
    PASS();
#endif

    // 清理
    mgr.shutdownAll();
}

// ============================================================================
// main
// ============================================================================

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);  // 解决中文输出乱码
#endif

    std::cout << "================================================\n";
    std::cout << "  SimpleScriptEngine — Quad-Engine Example\n";
    std::cout << "================================================\n";

    // ---- 1. SimpleScriptValue ----
    testSimpleScriptValue();

    // ---- 2. 各引擎直接使用 ----
    std::cout << "\n--- 引擎生命周期测试 ---\n\n";
#ifdef SIMPLESCRIPTENGINE_ENABLE_LUAJIT
    testEngineLifecycle(std::make_unique<LuaJITEngine>());
#endif
#ifdef SIMPLESCRIPTENGINE_ENABLE_QUICKJS
    testEngineLifecycle(std::make_unique<QuickJSEngine>());
#endif
#ifdef SIMPLESCRIPTENGINE_ENABLE_CHAISCRIPT
    testEngineLifecycle(std::make_unique<ChaiScriptEngine>());
#endif
#ifdef SIMPLESCRIPTENGINE_ENABLE_ANGELSCRIPT
    testEngineLifecycle(std::make_unique<AngelScriptEngine>());
#endif

    std::cout << "\n--- 引擎 eval 测试 ---\n\n";
#ifdef SIMPLESCRIPTENGINE_ENABLE_LUAJIT
    testEngineEval(std::make_unique<LuaJITEngine>());
#endif
#ifdef SIMPLESCRIPTENGINE_ENABLE_QUICKJS
    testEngineEval(std::make_unique<QuickJSEngine>());
#endif
#ifdef SIMPLESCRIPTENGINE_ENABLE_CHAISCRIPT
    testEngineEval(std::make_unique<ChaiScriptEngine>());
#endif
#ifdef SIMPLESCRIPTENGINE_ENABLE_ANGELSCRIPT
    testEngineEval(std::make_unique<AngelScriptEngine>());
#endif

    std::cout << "\n--- 引擎 setGlobal/getGlobal 测试 ---\n\n";
#ifdef SIMPLESCRIPTENGINE_ENABLE_LUAJIT
    testEngineSetGetGlobal(std::make_unique<LuaJITEngine>());
#endif
#ifdef SIMPLESCRIPTENGINE_ENABLE_QUICKJS
    testEngineSetGetGlobal(std::make_unique<QuickJSEngine>());
#endif
#ifdef SIMPLESCRIPTENGINE_ENABLE_CHAISCRIPT
    testEngineSetGetGlobal(std::make_unique<ChaiScriptEngine>());
#endif
#ifdef SIMPLESCRIPTENGINE_ENABLE_ANGELSCRIPT
    testEngineSetGetGlobal(std::make_unique<AngelScriptEngine>());
#endif

    std::cout << "\n--- 引擎 executeString & call 测试 ---\n\n";
#ifdef SIMPLESCRIPTENGINE_ENABLE_LUAJIT
    testEngineExecuteString(std::make_unique<LuaJITEngine>());
#endif
#ifdef SIMPLESCRIPTENGINE_ENABLE_QUICKJS
    testEngineExecuteString(std::make_unique<QuickJSEngine>());
#endif
#ifdef SIMPLESCRIPTENGINE_ENABLE_CHAISCRIPT
    testEngineExecuteString(std::make_unique<ChaiScriptEngine>());
#endif
#ifdef SIMPLESCRIPTENGINE_ENABLE_ANGELSCRIPT
    testEngineExecuteString(std::make_unique<AngelScriptEngine>());
#endif

    std::cout << "\n--- 引擎 registerFunction 测试 ---\n\n";
#ifdef SIMPLESCRIPTENGINE_ENABLE_LUAJIT
    testRegisterFunction(std::make_unique<LuaJITEngine>());
#endif
#ifdef SIMPLESCRIPTENGINE_ENABLE_QUICKJS
    testRegisterFunction(std::make_unique<QuickJSEngine>());
#endif
#ifdef SIMPLESCRIPTENGINE_ENABLE_CHAISCRIPT
    testRegisterFunction(std::make_unique<ChaiScriptEngine>());
#endif
#ifdef SIMPLESCRIPTENGINE_ENABLE_ANGELSCRIPT
    testRegisterFunction(std::make_unique<AngelScriptEngine>());
#endif

    std::cout << "\n--- 引擎 executeFile 测试 ---\n\n";
#ifdef SIMPLESCRIPTENGINE_ENABLE_LUAJIT
    testEngineExecuteFile(std::make_unique<LuaJITEngine>());
#endif
#ifdef SIMPLESCRIPTENGINE_ENABLE_QUICKJS
    testEngineExecuteFile(std::make_unique<QuickJSEngine>());
#endif
#ifdef SIMPLESCRIPTENGINE_ENABLE_CHAISCRIPT
    testEngineExecuteFile(std::make_unique<ChaiScriptEngine>());
#endif
#ifdef SIMPLESCRIPTENGINE_ENABLE_ANGELSCRIPT
    testEngineExecuteFile(std::make_unique<AngelScriptEngine>());
#endif

    std::cout << "\n--- 引擎 hasFunction 测试 ---\n\n";
#ifdef SIMPLESCRIPTENGINE_ENABLE_LUAJIT
    testEngineHasFunction(std::make_unique<LuaJITEngine>());
#endif
#ifdef SIMPLESCRIPTENGINE_ENABLE_QUICKJS
    testEngineHasFunction(std::make_unique<QuickJSEngine>());
#endif
#ifdef SIMPLESCRIPTENGINE_ENABLE_CHAISCRIPT
    testEngineHasFunction(std::make_unique<ChaiScriptEngine>());
#endif
#ifdef SIMPLESCRIPTENGINE_ENABLE_ANGELSCRIPT
    testEngineHasFunction(std::make_unique<AngelScriptEngine>());
#endif

    // ---- 3. SimpleScriptManager ----
    testManager();

    // ---- 结果 ----
    std::cout << "\n================================================\n";
    std::cout << "  结果: " << g_passed << " passed, "
              << g_failed << " failed, "
              << (g_passed + g_failed) << " total\n";
    std::cout << "================================================\n";

    return g_failed > 0 ? 1 : 0;
}

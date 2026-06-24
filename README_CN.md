# SimpleScriptEngine — 统一 C++ 多脚本引擎抽象层

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![CMake](https://img.shields.io/badge/CMake-3.16%2B-green.svg)](https://cmake.org/)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg)]()

**SimpleScriptEngine** 将一个 C++ 项目同时嵌入 **LuaJIT**、**QuickJS**、**ChaiScript**、**AngelScript** 四个脚本引擎，通过统一抽象接口 `SimpleScriptEngine` 消除引擎差异，按需路由到最合适的引擎。

> 📖 [English Documentation](README.md)

---

## 目录

- [项目概述](#项目概述)
- [核心特性](#核心特性)
- [架构设计](#架构设计)
- [核心组件](#核心组件)
- [四个引擎对比](#四个引擎对比)
- [项目结构](#项目结构)
- [快速开始](#快速开始)
- [使用指南](#使用指南)
- [API 参考](#api-参考)
- [CMake 构建选项](#cmake-构建选项)
- [平台支持](#平台支持)
- [第三方依赖](#第三方依赖)
- [离线构建](#离线构建)
- [贡献指南](#贡献指南)
- [许可证](#许可证)

---

## 项目概述

在游戏引擎、科学计算、自动化工具等场景中，常常需要嵌入多种脚本语言以应对不同需求：

| 场景 | 最佳选择 | 原因 |
|------|----------|------|
| 热更新逻辑 | **LuaJIT** | JIT 编译，性能极高，内存占用极小 |
| 配置驱动开发 | **QuickJS** | 完整 ES2020 标准，JavaScript 生态丰富 |
| C++ 开发者脚本 | **ChaiScript** | 原生 C++ 语法，零学习成本 |
| 大型游戏逻辑 | **AngelScript** | 静态类型 + 编译期检查，游戏行业标准 |

**SimpleScriptEngine** 将这四个引擎封装在统一的 C++ 接口下，切换引擎只需修改一行代码，无需改动业务逻辑。

---

## 核心特性

- **统一 API** — 一个抽象接口（`SimpleScriptEngine`）适配全部四个引擎
- **跨引擎值类型** — `SimpleScriptValue` 屏蔽各引擎内部类型差异
- **智能路由** — `SimpleScriptManager` 按文件扩展名自动分发：`.lua` → LuaJIT, `.js` → QuickJS, `.chai` → ChaiScript, `.as` → AngelScript
- **双向调用** — C++ 调用脚本函数（`call`），脚本回调 C++ 函数（`registerFunction`）
- **跨引擎共享变量** — 在所有已注册引擎间共享全局变量
- **线程安全** — 全局管理器操作受 mutex 保护
- **按需编译** — 通过 CMake 选项仅启用需要的引擎
- **离线构建支持** — 可缓存第三方依赖，适应断网环境
- **PIMPL-Ready** — 纯虚接口确保 ABI 稳定

---

## 架构设计

```
┌──────────────────────────────────────────────────────────┐
│                  SimpleScriptManager                      │
│   (引擎注册、智能路由、跨引擎共享变量、批量生命周期管理)      │
├────────────┬────────────┬────────────┬───────────────────┤
│  LuaJIT    │  QuickJS   │ ChaiScript │  AngelScript      │
│  引擎适配器  │  引擎适配器  │  引擎适配器  │  引擎适配器          │
├────────────┼────────────┼────────────┼───────────────────┤
│  LuaJIT    │  QuickJS   │ ChaiScript │  AngelScript      │
│  原生库     │  原生库     │  原生库     │  原生库              │
└────────────┴────────────┴────────────┴───────────────────┘
                               │
               SimpleScriptValue (跨引擎统一值类型)
```

---

## 核心组件

### `SimpleScriptValue` — 跨引擎统一值类型

屏蔽各引擎内部类型差异，支持 **八种类型**：Null、Bool、Int、Double、String、Array、Object、Function。值语义设计，轻量级适合频繁传参。

```cpp
using namespace SimpleScriptEngine;

// 工厂方法创建值
auto intVal  = SimpleScriptValue::Int(42);
auto strVal  = SimpleScriptValue::Str("hello");
auto boolVal = SimpleScriptValue::Bool(true);
auto numVal  = SimpleScriptValue::Num(3.14);
auto arrVal  = SimpleScriptValue::Arr({ SimpleScriptValue::Int(1), SimpleScriptValue::Int(2) });
auto objVal  = SimpleScriptValue::Obj({ {"key", SimpleScriptValue::Str("val")} });
auto nullVal = SimpleScriptValue::Null();

// 类型查询
intVal.isInt();       // true
intVal.isNumber();    // true
numVal.isDouble();    // true
arrVal.isArray();     // true

// 值提取
int64_t v  = intVal.asInt();        // 42
double  d  = numVal.asDouble();     // 3.14
std::string s = strVal.asString();  // "hello"

// 类型转换
std::string str = intVal.toString();  // "42"

// 拷贝与移动语义（轻量级）
auto copy  = intVal;               // 深拷贝
auto moved = std::move(copy);      // 移动
```

### `SimpleScriptEngine` — 统一抽象接口

纯虚基类，定义所有引擎适配器必须实现的标准接口：

| 类别 | 方法 | 说明 |
|------|------|------|
| **元信息** | `engineName()` | 引擎可读名称（如 `"LuaJIT"`）— `noexcept` |
| | `engineVersion()` | 引擎版本字符串 — `noexcept` |
| | `fileExtension()` | 默认文件扩展名（如 `".lua"`）— `noexcept` |
| **生命周期** | `initialize()` | 初始化引擎运行时 |
| | `shutdown()` | 关闭引擎并释放资源 |
| | `isInitialized()` | 检查引擎是否就绪 — `noexcept` |
| **脚本执行** | `executeString(code)` | 执行一段脚本代码 |
| | `executeFile(path)` | 从文件加载并执行脚本 |
| | `eval(expr)` | 求值表达式并返回结果 |
| **C++ → 脚本** | `call(func, args)` | 从 C++ 调用脚本函数 |
| | `hasFunction(name)` | 检查可调用函数是否存在 |
| | `setGlobal(name, val)` | 从 C++ 设置全局变量 |
| | `getGlobal(name)` | 从 C++ 读取全局变量 |
| **脚本 → C++** | `registerFunction(name, fn)` | 向脚本注册 C++ 函数 |
| **错误处理** | `setErrorCallback(cb)` | 设置脚本错误回调 |

### `SimpleScriptManager` — 统一管理器

单例管理器，提供引擎注册、智能路由、跨引擎共享全局变量和批量生命周期管理（线程安全）。

| 功能 | 说明 |
|------|------|
| **引擎注册** | `registerEngine()` / `unregisterEngine()` / `getEngine()` / `listEngines()` |
| **按名称执行** | `executeString("LuaJIT", code)` — 在指定引擎上执行 |
| **按扩展名自动路由** | `executeFile("main.lua")` — `.lua` → LuaJIT, `.js` → QuickJS, `.chai` → ChaiScript, `.as` → AngelScript |
| **跨引擎共享变量** | `setSharedGlobal()` / `getSharedGlobal()` / `hasSharedGlobal()` |
| **批量生命周期** | `initializeAll()` / `shutdownAll()` — 一次性管理所有引擎 |
| **线程安全** | 所有操作均受 mutex 保护 |

---

## 四个引擎对比

| 引擎 | 语言 | 版本 | 类型系统 | 优势 |
|------|------|------|----------|------|
| **LuaJIT** | Lua 5.1 | master | 动态 | JIT 编译，性能极高，FFI 支持，适合热更新 |
| **QuickJS** | ES2020 | 0.15.1 | 动态 | 轻量级，内存占用小，完整 ES2020 标准，支持 ES Module |
| **ChaiScript** | C++-like | 6.1.0 | 动态 | 原生 C++ 语法，header-only，零额外依赖 |
| **AngelScript** | C/C++-like | 2.38.0 | 静态 | 编译期类型检查，游戏行业标准，丰富的 add-on 库 |

### 引擎特性一览

| 特性 | LuaJIT | QuickJS | ChaiScript | AngelScript |
|------|--------|---------|------------|-------------|
| 执行速度 | ★★★★★ | ★★★☆☆ | ★★☆☆☆ | ★★★★☆ |
| 内存占用 | ★★★★★ | ★★★★★ | ★★★☆☆ | ★★★★☆ |
| C++ 互操作 | ★★★★☆ | ★★★☆☆ | ★★★★★ | ★★★★☆ |
| 类型安全 | ★★☆☆☆ | ★★☆☆☆ | ★★☆☆☆ | ★★★★★ |
| 学习曲线（低=好） | ★★★★☆ | ★★★★★ | ★★★★★ | ★★★☆☆ |
| 模块系统 | `dofile()` | ES Modules (`import/export`) | `use()` | `#include` |

---

## 项目结构

```
SimpleScriptEngine/
├── cmake/
│   ├── DownloadUnzipProject.cmake       # 下载和解压工具模块
│   └── ExecuteDownloadProjects.cmake    # 第三方依赖下载配置
├── examples/
│   └── SimpleScriptEngineExample/
│       ├── CMakeLists.txt
│       └── main.cpp                     # 完整使用示例（含测试用例）
├── src/
│   └── SimpleScriptEngine/
│       ├── CMakeLists.txt               # 核心库构建脚本
│       ├── SimpleScriptEngine.h         # 抽象接口（纯虚基类）
│       ├── SimpleScriptValue.h          # 跨引擎统一值类型
│       ├── SimpleScriptManager.h        # 单例管理器 + 智能路由
│       ├── SimpleScriptManager.cpp
│       └── Engines/
│           ├── LuaJITEngine.h           # LuaJIT 适配器
│           ├── LuaJITEngine.cpp
│           ├── QuickJSEngine.h          # QuickJS 适配器
│           ├── QuickJSEngine.cpp
│           ├── ChaiScriptEngine.h       # ChaiScript 适配器
│           ├── ChaiScriptEngine.cpp
│           ├── AngelScriptEngine.h      # AngelScript 适配器
│           └── AngelScriptEngine.cpp
├── 3rdparty/                            # 下载的依赖源码（自动生成）
├── 3rdparty-download/                   # 缓存的压缩包（自动生成）
├── CMakeLists.txt                       # 顶层 CMake
├── VERSION
├── LICENSE
├── README.md                            # 英文文档
└── README_CN.md                         # 中文文档（本文件）
```

---

## 快速开始

### 环境要求

- **CMake** ≥ 3.16
- **编译器**：支持 C++17（MSVC 2019+、GCC 9+、Clang 10+）
- **网络连接**：首次构建时自动下载第三方依赖

### 克隆和构建

```bash
git clone <repository-url>
cd SimpleScriptEngine

# 配置（默认启用全部四个引擎）
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 构建
cmake --build build --config Release

# 运行示例程序
./build/examples/SimpleScriptEngineExample/Release/SimpleScriptEngineExample
```

### 仅启用部分引擎

```bash
# 仅启用 LuaJIT 和 QuickJS
cmake -B build \
    -DSIMPLESCRIPTENGINE_ENABLE_LUAJIT=ON \
    -DSIMPLESCRIPTENGINE_ENABLE_QUICKJS=ON \
    -DSIMPLESCRIPTENGINE_ENABLE_CHAISCRIPT=OFF \
    -DSIMPLESCRIPTENGINE_ENABLE_ANGELSCRIPT=OFF
```

### 构建为动态库

```bash
cmake -B build -DSIMPLESCRIPTENGINE_BUILD_SHARED=ON
cmake --build build --config Release
```

### 集成到你的 CMake 项目

安装后可在自己的项目中使用：

```cmake
find_package(SimpleScriptEngine REQUIRED)
target_link_libraries(your_target PRIVATE SimpleScriptEngine::SimpleScriptEngine)
```

或作为子目录引入：

```cmake
add_subdirectory(path/to/SimpleScriptEngine)
target_link_libraries(your_target PRIVATE SimpleScriptEngine)
```

---

## 使用指南

### 快速示例：直接使用引擎

```cpp
#include "SimpleScriptEngine.h"
#include "Engines/LuaJITEngine.h"

using namespace SimpleScriptEngine;

// 创建并初始化引擎
auto engine = std::make_unique<LuaJITEngine>();
engine->initialize();

// 执行脚本代码
engine->executeString("x = 42");

// 从 C++ 设置全局变量
engine->setGlobal("message", SimpleScriptValue::Str("Hello from C++"));

// 从 C++ 调用脚本函数
engine->executeString("function add(a, b) return a + b end");
auto result = engine->call("add", {
    SimpleScriptValue::Int(3),
    SimpleScriptValue::Int(4)
});
std::cout << result.asInt() << std::endl;  // 7

// 向脚本注册 C++ 函数
engine->registerFunction("cpp_log", [](const std::vector<SimpleScriptValue>& args) {
    if (!args.empty()) std::cout << "[Script] " << args[0].toString() << std::endl;
    return SimpleScriptValue::Null();
});

// 表达式求值
auto val = engine->eval("10 * 5 + 3");
std::cout << val.asInt() << std::endl;  // 53

// 执行脚本文件
engine->executeFile("scripts/main.lua");

// 检查函数是否存在
if (engine->hasFunction("add")) {
    auto sum = engine->call("add", { SimpleScriptValue::Int(10), SimpleScriptValue::Int(20) });
    std::cout << sum.asInt() << std::endl;  // 30
}

// 清理
engine->shutdown();
```

### 使用 SimpleScriptManager 智能路由

```cpp
#include "SimpleScriptManager.h"
#include "Engines/LuaJITEngine.h"
#include "Engines/QuickJSEngine.h"

using namespace SimpleScriptEngine;

auto& mgr = SimpleScriptManager::instance();

// 注册引擎
mgr.registerEngine(std::make_unique<LuaJITEngine>());
mgr.registerEngine(std::make_unique<QuickJSEngine>());

// 批量初始化所有已注册引擎
mgr.initializeAll();

// 按引擎名称执行
mgr.executeString("LuaJIT", "print('Hello Lua')");
mgr.executeString("QuickJS", "console.log('Hello JS')");

// 按文件扩展名自动路由
mgr.executeFile("scripts/main.lua");   // → LuaJIT
mgr.executeFile("scripts/config.js");  // → QuickJS

// 跨引擎共享全局变量
mgr.setSharedGlobal("app_name", SimpleScriptValue::Str("MyApp"));
auto name = mgr.getSharedGlobal("app_name");
std::cout << name.asString() << std::endl;  // "MyApp"

// 列出所有已注册引擎
for (const auto& eng : mgr.listEngines()) {
    std::cout << "引擎: " << eng << std::endl;
}

// 按扩展名查找引擎
auto* jsEngine = mgr.findEngineByExtension(".js");

// 清理
mgr.shutdownAll();
```

### 脚本模块导入（各引擎原生方式）

每个引擎支持通过原生机制加载子模块脚本：

| 引擎 | 机制 | 示例 |
|------|------|------|
| **LuaJIT** | `dofile()` | `local mod = dofile("module.lua")` |
| **QuickJS** | ES Modules | `import { fn } from "./module.js"` |
| **ChaiScript** | `use()` | `use("module.chai")` |
| **AngelScript** | `#include` | `#include "module.as"` |

### 错误回调

```cpp
engine->setErrorCallback([](const std::string& engineName, const std::string& message) {
    std::cerr << "[" << engineName << " 错误] " << message << std::endl;
});
```

### 最佳实践

1. **`initialize()` 与 `shutdown()` 配对使用** — 考虑使用 RAII 封装
2. **使用前检查 `isInitialized()`** — 尤其在 `initialize()` 返回后
3. **`call()` 前检查 `hasFunction()`** — 避免函数不存在时崩溃
4. **设置错误回调** — 开发阶段及早捕获脚本错误
5. **多引擎项目优先使用 `SimpleScriptManager`** — 自动处理路由和生命周期
6. **使用 `SimpleScriptValue` 工厂方法**（`Int()`、`Str()` 等）而非直接构造函数，提高可读性

---

## API 参考

### SimpleScriptValue 类型

```cpp
enum class ScriptValueType { Null, Bool, Int, Double, String, Array, Object, Function };

// 类型别名
using ScriptFunction = std::function<SimpleScriptValue(const std::vector<SimpleScriptValue>&)>;
using ScriptObject   = std::unordered_map<std::string, SimpleScriptValue>;
using ScriptArray    = std::vector<SimpleScriptValue>;
```

### SimpleScriptEngine 接口（抽象基类）

```cpp
class SimpleScriptEngine {
public:
    virtual ~SimpleScriptEngine() = default;

    // 元信息（始终可用 — noexcept）
    virtual const char* engineName()    const noexcept = 0;  // 如 "LuaJIT"
    virtual const char* engineVersion() const noexcept = 0;  // 如 "2.1.0"
    virtual const char* fileExtension() const noexcept = 0;  // 如 ".lua"

    // 生命周期管理
    virtual bool initialize() = 0;
    virtual void shutdown()   = 0;
    virtual bool isInitialized() const noexcept = 0;

    // 脚本执行
    virtual bool              executeString(const std::string& code) = 0;
    virtual bool              executeFile(const std::string& path)   = 0;
    virtual SimpleScriptValue eval(const std::string& expr)          = 0;

    // C++ → 脚本
    virtual bool              hasFunction(const std::string& name) = 0;
    virtual SimpleScriptValue call(const std::string& func,
                                   const std::vector<SimpleScriptValue>& args = {}) = 0;
    virtual void              setGlobal(const std::string& name, const SimpleScriptValue& value) = 0;
    virtual SimpleScriptValue getGlobal(const std::string& name) = 0;

    // 脚本 → C++
    virtual void registerFunction(const std::string& name, ScriptFunction func) = 0;

    // 错误处理
    using ErrorCallback = std::function<void(const std::string& engineName,
                                              const std::string& message)>;
    virtual void setErrorCallback(ErrorCallback cb);
};
```

### SimpleScriptManager（单例管理器）

```cpp
class SimpleScriptManager {
public:
    static SimpleScriptManager& instance();

    // 引擎注册
    bool registerEngine(std::unique_ptr<SimpleScriptEngine> engine);
    bool unregisterEngine(const std::string& name);
    SimpleScriptEngine* getEngine(const std::string& name);
    SimpleScriptEngine* findEngineByExtension(const std::string& ext);
    std::vector<std::string> listEngines() const;

    // 批量生命周期管理
    size_t initializeAll();
    void   shutdownAll();

    // 智能路由执行
    bool executeString(const std::string& engineName, const std::string& code);
    bool executeFile(const std::string& path);
    bool hasFunction(const std::string& engineName, const std::string& funcName);
    SimpleScriptValue call(const std::string& engineName, const std::string& funcName,
                           const std::vector<SimpleScriptValue>& args = {});

    // 跨引擎共享全局变量
    void setSharedGlobal(const std::string& name, const SimpleScriptValue& value);
    SimpleScriptValue getSharedGlobal(const std::string& name);
    bool hasSharedGlobal(const std::string& name);
};
```

---

## CMake 构建选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `SIMPLESCRIPTENGINE_ENABLE_LUAJIT` | `ON` | 启用 LuaJIT 引擎 |
| `SIMPLESCRIPTENGINE_ENABLE_QUICKJS` | `ON` | 启用 QuickJS 引擎 |
| `SIMPLESCRIPTENGINE_ENABLE_CHAISCRIPT` | `ON` | 启用 ChaiScript 引擎 |
| `SIMPLESCRIPTENGINE_ENABLE_ANGELSCRIPT` | `ON` | 启用 AngelScript 引擎 |
| `SIMPLESCRIPTENGINE_BUILD_EXAMPLES` | `ON` | 构建示例程序 |
| `SIMPLESCRIPTENGINE_BUILD_SHARED` | `OFF` | 构建为共享库（默认静态库） |
| `ENABLE_PROXY` | `OFF` | 启用代理下载第三方依赖 |
| `DEFAULT_HTTP_PROXY` | (空) | 默认 HTTP 代理地址 |
| `DEFAULT_HTTPS_PROXY` | (空) | 默认 HTTPS 代理地址 |

---

## 平台支持

| 平台 | 编译器 | 状态 |
|------|--------|------|
| **Windows** | MSVC 2019+ | ✅ 支持 |
| **Linux** | GCC 9+, Clang 10+ | ✅ 支持 |
| **macOS** | Clang 10+ | ✅ 支持 |

> **注意：** Windows 下示例程序会自动将控制台输出编码设置为 UTF-8，以正确处理国际化字符。

---

## 第三方依赖

所有依赖在 CMake 配置阶段自动下载，无需手动安装。

| 依赖 | 来源 | 版本 | 用途 | 许可证 |
|------|------|------|------|--------|
| **luajit-rocks** | [torch/luajit-rocks](https://github.com/torch/luajit-rocks) | master | LuaJIT 运行时 | MIT |
| **quickjs-ng** | [quickjs-ng/quickjs](https://github.com/quickjs-ng/quickjs) | 0.15.1 | QuickJS 运行时 | MIT |
| **ChaiScript** | [ChaiScript/ChaiScript](https://github.com/ChaiScript/ChaiScript) | 6.1.0 | ChaiScript 运行时（header-only） | BSD |
| **AngelScript** | [anjo76/angelscript](https://github.com/anjo76/angelscript) | 2.38.0 | AngelScript 运行时 | zlib |

依赖源码解压到 `3rdparty/` 目录，压缩包缓存在 `3rdparty-download/` 目录。

---

## 离线构建

如果构建机器无网络连接：

1. 在有网络的环境执行一次 CMake 配置，生成 `3rdparty/` 和 `3rdparty-download/` 目录
2. 将整个项目（包含上述目录）复制到离线环境
3. 正常构建 — CMake 会检测到已经缓存的依赖，跳过下载步骤

---

## 贡献指南

欢迎提交 Issue 和 Pull Request！

### 开发环境搭建

```bash
git clone <repository-url>
cd SimpleScriptEngine
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DSIMPLESCRIPTENGINE_BUILD_EXAMPLES=ON
cmake --build build --config Debug
```

### 代码风格

- C++17 标准，扩展已禁用
- 头文件保护使用 `#pragma once`
- 命名空间：`SimpleScriptEngine`
- 公开 API 使用 Doxygen 风格注释

---

## 许可证

本项目采用 [MIT](LICENSE) 许可证。

四个集成的脚本引擎各自遵守其原始许可证：

| 引擎 | 许可证 |
|------|--------|
| LuaJIT | MIT |
| QuickJS | MIT |
| ChaiScript | BSD |
| AngelScript | zlib |

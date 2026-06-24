# SimpleScriptEngine — 统一 C++ 多脚本引擎抽象层

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![CMake](https://img.shields.io/badge/CMake-3.16%2B-green.svg)](https://cmake.org/)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

在一个 C++ 项目中同时嵌入 **LuaJIT**、**QuickJS**、**ChaiScript**、**AngelScript** 四个脚本引擎，通过统一抽象接口 `SimpleScriptEngine` 消除引擎差异，按需路由到最合适的引擎。

---

## 目录

- [项目背景](#项目背景)
- [架构](#架构)
- [核心组件](#核心组件)
- [四个引擎](#四个引擎)
- [快速开始](#快速开始)
- [Usage](#Usage)
- [CMake 构建选项](#cmake-构建选项)
- [第三方依赖](#第三方依赖)
- [许可证](#许可证)

---

## 项目背景

在游戏引擎、科学计算、自动化工具等场景中，常常需要嵌入多种脚本语言来应对不同需求：

- **Lua** 速度快、嵌入简单，适合性能敏感的热更新逻辑
- **JavaScript** 生态丰富，适合配置驱动和前端联调
- **ChaiScript** 语法接近 C++，降低学习成本
- **AngelScript** 静态类型 + 编译期检查，适合大型游戏逻辑

**SimpleScriptEngine** 将这些引擎封装在统一的 C++ 接口下，让上层代码无需关心底层使用哪个引擎。切换引擎只需修改一行代码，无需改动业务逻辑。

---

## 架构

```
┌──────────────────────────────────────────────────┐
│                  SimpleScriptManager              │
│   (统一管理、自动路由、跨引擎全局变量、线程安全)      │
├──────────┬──────────┬──────────┬─────────────────┤
│ LuaJIT   │ QuickJS  │ChaiScript│ AngelScript     │
│ 引擎适配器│ 引擎适配器│ 引擎适配器│ 引擎适配器        │
├──────────┼──────────┼──────────┼─────────────────┤
│ LuaJIT   │ QuickJS  │ChaiScript│ AngelScript     │
│ 原生库   │ 原生库   │ 原生库   │ 原生库            │
└──────────┴──────────┴──────────┴─────────────────┘
```

---

## 核心组件

### SimpleScriptValue — 跨引擎统一值类型

屏蔽各引擎内部类型差异，支持 Null、Bool、Int、Double、String、Array、Object、Function 八种类型，值语义设计，轻量级适合频繁传参。

```cpp
using namespace SimpleScriptEngine;

auto intVal    = SimpleScriptValue::Int(42);
auto strVal    = SimpleScriptValue::Str("hello");
auto boolVal   = SimpleScriptValue::Bool(true);
auto arrVal    = SimpleScriptValue::Arr({ SimpleScriptValue::Int(1), SimpleScriptValue::Int(2) });
auto objVal    = SimpleScriptValue::Obj({ {"key", SimpleScriptValue::Str("val")} });

int64_t v = intVal.asInt();          // 42
std::string s = intVal.toString();   // "42"
```

### SimpleScriptEngine — 统一抽象接口

纯虚基类，定义所有引擎适配器必须实现的标准接口：

| 接口 | 说明 |
|------|------|
| `engineName()` / `engineVersion()` / `fileExtension()` | 引擎元信息（noexcept） |
| `initialize()` / `shutdown()` / `isInitialized()` | 生命周期管理 |
| `executeString()` / `executeFile()` | 脚本执行 |
| `eval()` | 表达式求值 |
| `call()` | C++ 调用脚本函数 |
| `setGlobal()` / `getGlobal()` | 变量绑定 |
| `registerFunction()` | 向脚本注册 C++ 函数 |
| `setErrorCallback()` | 错误回调 |

### SimpleScriptManager — 统一管理器

单例管理器，提供引擎注册、智能路由、跨引擎共享全局变量和批量生命周期管理（线程安全）。

- 按引擎名称执行：`executeString("LuaJIT", code)`
- 按文件扩展名自动路由：`.lua` → LuaJIT, `.js` → QuickJS, `.chai` → ChaiScript, `.as` → AngelScript
- 跨引擎共享全局变量：`setSharedGlobal("name", value)`
- 批量管理：`initializeAll()` / `shutdownAll()`

---

## 四个引擎

| 引擎 | 语言 | 版本 | 特点 |
|------|------|------|------|
| **LuaJIT** | Lua 5.1 | master | JIT 编译，性能极高，FFI 支持，适合热更新 |
| **QuickJS** | ES2020 | 0.15.1 | 轻量级，内存占用小，完整 ES2020 标准 |
| **ChaiScript** | C++-like | 6.1.0 | 原生 C++ 语法，header-only，零额外依赖 |
| **AngelScript** | C/C++-like | 2.38.0 | 静态类型，编译期检查，游戏行业标准 |

---

## 快速开始

### 环境要求

- **CMake** ≥ 3.16
- **编译器**：支持 C++17（MSVC 2019+、GCC 9+、Clang 10+）
- 构建时自动下载第三方依赖（需要网络连接）

### 克隆 & 构建

```bash
git clone <repository-url>
cd SimpleScriptEngine

# 配置（默认启用全部四个引擎）
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 构建
cmake --build build --config Release

# 运行示例
./build/examples/SimpleScriptEngineExample/Release/SimpleScriptEngineExample
```

### 只启用部分引擎

```bash
cmake -B build \
    -DSIMPLESCRIPTENGINE_ENABLE_LUAJIT=ON \
    -DSIMPLESCRIPTENGINE_ENABLE_QUICKJS=ON \
    -DSIMPLESCRIPTENGINE_ENABLE_CHAISCRIPT=OFF \
    -DSIMPLESCRIPTENGINE_ENABLE_ANGELSCRIPT=OFF
```

---

## Usage

### 直接使用某个引擎

```cpp
#include "SimpleScriptEngine.h"
#include "Engines/LuaJITEngine.h"

using namespace SimpleScriptEngine;

auto engine = std::make_unique<LuaJITEngine>();
engine->initialize();

// 执行脚本
engine->executeString("x = 42");

// 设置变量
engine->setGlobal("message", SimpleScriptValue::Str("Hello from C++"));

// 调用脚本函数
engine->executeString("function add(a, b) return a + b end");
auto result = engine->call("add", {
    SimpleScriptValue::Int(3),
    SimpleScriptValue::Int(4)
});
std::cout << result.asInt() << std::endl; // 7

// 注册 C++ 函数供脚本调用
engine->registerFunction("cpp_log", [](const std::vector<SimpleScriptValue>& args) {
    if (!args.empty()) std::cout << "[Script] " << args[0].toString() << std::endl;
    return SimpleScriptValue::Null();
});

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

// 批量初始化
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
std::cout << name.asString() << std::endl; // "MyApp"

// 清理
mgr.shutdownAll();
```

### 错误回调

```cpp
engine->setErrorCallback([](const std::string& engine, const std::string& msg) {
    std::cerr << "[" << engine << " Error] " << msg << std::endl;
});
```

---

## CMake 构建选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `SIMPLESCRIPTENGINE_ENABLE_LUAJIT` | ON | 启用 LuaJIT 引擎 |
| `SIMPLESCRIPTENGINE_ENABLE_QUICKJS` | ON | 启用 QuickJS 引擎 |
| `SIMPLESCRIPTENGINE_ENABLE_CHAISCRIPT` | ON | 启用 ChaiScript 引擎 |
| `SIMPLESCRIPTENGINE_ENABLE_ANGELSCRIPT` | ON | 启用 AngelScript 引擎 |
| `SIMPLESCRIPTENGINE_BUILD_EXAMPLES` | ON | 构建示例程序 |
| `SIMPLESCRIPTENGINE_BUILD_SHARED` | OFF | 构建为共享库（默认静态库） |
| `ENABLE_PROXY` | OFF | 启用代理下载第三方依赖 |
| `DEFAULT_HTTP_PROXY` | (空) | 默认 HTTP 代理地址 |
| `DEFAULT_HTTPS_PROXY` | (空) | 默认 HTTPS 代理地址 |

---

## 第三方依赖

构建时通过 CMake 自动下载以下依赖，无需手动安装：

| 依赖 | 版本 | 用途 | 许可证 |
|------|------|------|--------|
| [luajit-rocks](https://github.com/torch/luajit-rocks) | master | LuaJIT 运行时 | MIT |
| [quickjs-ng](https://github.com/quickjs-ng/quickjs) | 0.15.1 | QuickJS 运行时 | MIT |
| [ChaiScript](https://github.com/ChaiScript/ChaiScript) | 6.1.0 | ChaiScript 运行时（header-only） | BSD |
| [AngelScript](https://github.com/anjo76/angelscript) | 2.38.0 | AngelScript 运行时 | zlib |

依赖下载到 `3rdparty/` 目录，压缩包缓存在 `3rdparty-download/` 目录。

### 离线构建

如果构建机器无网络连接，可先在有网络的环境执行一次 CMake 配置生成 `3rdparty/` 和 `3rdparty-download/` 目录，然后将整个项目复制到离线环境构建。

---

## 许可证

本项目采用 [MIT](LICENSE) 许可证。

四个集成的脚本引擎各自遵守其原始许可证：

- **LuaJIT** — MIT
- **QuickJS** — MIT
- **ChaiScript** — BSD
- **AngelScript** — zlib

---

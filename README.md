# SimpleScriptEngine — Unified C++ Multi-Scripting Engine Abstraction Layer

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![CMake](https://img.shields.io/badge/CMake-3.16%2B-green.svg)](https://cmake.org/)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg)]()

**SimpleScriptEngine** embeds **LuaJIT**, **QuickJS**, **ChaiScript**, and **AngelScript** — four scripting engines — into a single C++ project behind a unified abstraction interface, eliminating engine-specific differences and routing execution to the most appropriate engine on demand.

> 📖 [中文文档 / Chinese Documentation](README_CN.md)

---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Architecture](#architecture)
- [Core Components](#core-components)
- [The Four Engines](#the-four-engines)
- [Project Structure](#project-structure)
- [Quick Start](#quick-start)
- [Usage](#usage)
- [API Reference](#api-reference)
- [CMake Build Options](#cmake-build-options)
- [Platform Support](#platform-support)
- [Third-Party Dependencies](#third-party-dependencies)
- [Offline Build](#offline-build)
- [Contributing](#contributing)
- [License](#license)

---

## Overview

In game engines, scientific computing, automation tools, and similar domains, embedding multiple scripting languages is often necessary to address different needs:

| Scenario | Best Fit | Reason |
|----------|----------|--------|
| Hot-reload logic | **LuaJIT** | JIT-compiled, extremely fast, minimal footprint |
| Configuration-driven workflows | **QuickJS** | Full ES2020 support, rich JavaScript ecosystem |
| C++ developer scripting | **ChaiScript** | Native C++-like syntax, zero learning curve |
| Large-scale game logic | **AngelScript** | Static typing, compile-time checks, industry standard |

**SimpleScriptEngine** wraps all four engines under a uniform C++ interface. Switching engines requires changing a single line of code — no business logic modifications needed.

---

## Features

- **Unified API** — One abstract interface (`SimpleScriptEngine`) for all four engines
- **Cross-Engine Value Type** — `SimpleScriptValue` shields internal type differences across engines
- **Smart Routing** — `SimpleScriptManager` automatically dispatches `.lua` → LuaJIT, `.js` → QuickJS, `.chai` → ChaiScript, `.as` → AngelScript
- **Bidirectional Communication** — C++ calls script functions (`call`), scripts call C++ functions (`registerFunction`)
- **Cross-Engine Shared Globals** — Share variables across all registered engines
- **Thread Safety** — Global manager operations are mutex-protected
- **Modular Build** — Enable only the engines you need via CMake options
- **Offline Build Support** — Cache third-party dependencies for air-gapped environments
- **PIMPL-Ready** — Pure virtual interface ensures ABI stability

---

## Architecture

```
┌──────────────────────────────────────────────────────────┐
│                  SimpleScriptManager                      │
│   (Registration, Smart Routing, Shared Globals, Batch Mgmt)│
├────────────┬────────────┬────────────┬───────────────────┤
│  LuaJIT    │  QuickJS   │ ChaiScript │  AngelScript      │
│  Adapter   │  Adapter   │  Adapter   │  Adapter          │
├────────────┼────────────┼────────────┼───────────────────┤
│  LuaJIT    │  QuickJS   │ ChaiScript │  AngelScript      │
│  Native    │  Native    │  Native    │  Native           │
└────────────┴────────────┴────────────┴───────────────────┘
                               │
               SimpleScriptValue (Unified Value Type)
```

---

## Core Components

### `SimpleScriptValue` — Cross-Engine Unified Value Type

Shields internal type differences between engines. Supports **eight types**: Null, Bool, Int, Double, String, Array, Object, and Function. Designed with value semantics — lightweight and suitable for frequent parameter passing.

```cpp
using namespace SimpleScriptEngine;

// Factory methods
auto intVal  = SimpleScriptValue::Int(42);
auto strVal  = SimpleScriptValue::Str("hello");
auto boolVal = SimpleScriptValue::Bool(true);
auto numVal  = SimpleScriptValue::Num(3.14);
auto arrVal  = SimpleScriptValue::Arr({ SimpleScriptValue::Int(1), SimpleScriptValue::Int(2) });
auto objVal  = SimpleScriptValue::Obj({ {"key", SimpleScriptValue::Str("val")} });
auto nullVal = SimpleScriptValue::Null();

// Type queries
intVal.isInt();       // true
intVal.isNumber();    // true
numVal.isDouble();    // true
arrVal.isArray();     // true

// Value extraction
int64_t v  = intVal.asInt();        // 42
double  d  = numVal.asDouble();     // 3.14
std::string s = strVal.asString();  // "hello"

// Type conversion
std::string str = intVal.toString();  // "42"

// Copy & move semantics (lightweight)
auto copy  = intVal;               // deep copy
auto moved = std::move(copy);      // move
```

### `SimpleScriptEngine` — Unified Abstract Interface

A pure virtual base class that defines the standard interface all engine adapters must implement:

| Category | Method | Description |
|----------|--------|-------------|
| **Metadata** | `engineName()` | Engine readable name (e.g., `"LuaJIT"`) — `noexcept` |
| | `engineVersion()` | Engine version string — `noexcept` |
| | `fileExtension()` | Default file extension (e.g., `".lua"`) — `noexcept` |
| **Lifecycle** | `initialize()` | Initialize the engine runtime |
| | `shutdown()` | Shut down and release all resources |
| | `isInitialized()` | Check if engine is ready — `noexcept` |
| **Execution** | `executeString(code)` | Execute a string of script code |
| | `executeFile(path)` | Load and execute a script file |
| | `eval(expr)` | Evaluate an expression and return the result |
| **C++ → Script** | `call(func, args)` | Call a script function from C++ |
| | `hasFunction(name)` | Check if a callable function exists |
| | `setGlobal(name, val)` | Set a global variable from C++ |
| | `getGlobal(name)` | Read a global variable from C++ |
| **Script → C++** | `registerFunction(name, fn)` | Register a C++ function for script use |
| **Error** | `setErrorCallback(cb)` | Set an error callback for script errors |

### `SimpleScriptManager` — Unified Manager

A singleton manager that provides engine registration, smart routing, cross-engine shared globals, and batch lifecycle management (thread-safe).

| Feature | Description |
|---------|-------------|
| **Engine Registration** | `registerEngine()` / `unregisterEngine()` / `getEngine()` / `listEngines()` |
| **Name-based Execution** | `executeString("LuaJIT", code)` — execute on a specific engine |
| **Extension-based Auto-Routing** | `executeFile("main.lua")` — auto-dispatches `.lua` → LuaJIT, `.js` → QuickJS, `.chai` → ChaiScript, `.as` → AngelScript |
| **Shared Globals** | `setSharedGlobal()` / `getSharedGlobal()` / `hasSharedGlobal()` — share variables across all engines |
| **Batch Lifecycle** | `initializeAll()` / `shutdownAll()` — manage all engines at once |
| **Thread Safety** | All operations are protected by a mutex |

---

## The Four Engines

| Engine | Language | Version | Type System | Strengths |
|--------|----------|---------|-------------|-----------|
| **LuaJIT** | Lua 5.1 | master | Dynamic | JIT compilation, extreme performance, FFI support, ideal for hot-reloading |
| **QuickJS** | ES2020 | 0.15.1 | Dynamic | Lightweight, small memory footprint, full ES2020 standard, ES module support |
| **ChaiScript** | C++-like | 6.1.0 | Dynamic | Native C++ syntax, header-only, zero external dependencies |
| **AngelScript** | C/C++-like | 2.38.0 | Static | Compile-time type checking, game industry standard, rich add-on library |

### Engine Comparison

| Feature | LuaJIT | QuickJS | ChaiScript | AngelScript |
|---------|--------|---------|------------|-------------|
| Execution Speed | ★★★★★ | ★★★☆☆ | ★★☆☆☆ | ★★★★☆ |
| Memory Footprint | ★★★★★ | ★★★★★ | ★★★☆☆ | ★★★★☆ |
| C++ Interop | ★★★★☆ | ★★★☆☆ | ★★★★★ | ★★★★☆ |
| Type Safety | ★★☆☆☆ | ★★☆☆☆ | ★★☆☆☆ | ★★★★★ |
| Learning Curve | ★★★★☆ | ★★★★★ | ★★★★★ | ★★★☆☆ |
| Module System | `dofile()` | ES Modules (`import/export`) | `use()` | `#include` |

---

## Project Structure

```
SimpleScriptEngine/
├── cmake/
│   ├── DownloadUnzipProject.cmake       # Download & unzip utilities
│   └── ExecuteDownloadProjects.cmake    # Third-party dependency downloads
├── examples/
│   └── SimpleScriptEngineExample/
│       ├── CMakeLists.txt
│       └── main.cpp                     # Comprehensive usage example with tests
├── src/
│   └── SimpleScriptEngine/
│       ├── CMakeLists.txt               # Core library build
│       ├── SimpleScriptEngine.h         # Abstract interface (pure virtual base)
│       ├── SimpleScriptValue.h          # Cross-engine unified value type
│       ├── SimpleScriptManager.h        # Singleton manager + smart routing
│       ├── SimpleScriptManager.cpp
│       └── Engines/
│           ├── LuaJITEngine.h           # LuaJIT adapter
│           ├── LuaJITEngine.cpp
│           ├── QuickJSEngine.h          # QuickJS adapter
│           ├── QuickJSEngine.cpp
│           ├── ChaiScriptEngine.h       # ChaiScript adapter
│           ├── ChaiScriptEngine.cpp
│           ├── AngelScriptEngine.h      # AngelScript adapter
│           └── AngelScriptEngine.cpp
├── 3rdparty/                            # Downloaded dependencies (auto-generated)
├── 3rdparty-download/                   # Cached zip archives (auto-generated)
├── CMakeLists.txt                       # Top-level CMake
├── VERSION
├── LICENSE
├── README.md
└── README_CN.md
```

---

## Quick Start

### Prerequisites

- **CMake** ≥ 3.16
- **Compiler**: C++17 support required (MSVC 2019+, GCC 9+, Clang 10+)
- **Internet connection** for initial build (third-party dependencies are downloaded automatically)

### Clone & Build

```bash
git clone <repository-url>
cd SimpleScriptEngine

# Configure (all four engines enabled by default)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release

# Run the example
./build/examples/SimpleScriptEngineExample/Release/SimpleScriptEngineExample
```

### Build with a Subset of Engines

```bash
# LuaJIT and QuickJS only
cmake -B build \
    -DSIMPLESCRIPTENGINE_ENABLE_LUAJIT=ON \
    -DSIMPLESCRIPTENGINE_ENABLE_QUICKJS=ON \
    -DSIMPLESCRIPTENGINE_ENABLE_CHAISCRIPT=OFF \
    -DSIMPLESCRIPTENGINE_ENABLE_ANGELSCRIPT=OFF
```

### Build as Shared Library

```bash
cmake -B build -DSIMPLESCRIPTENGINE_BUILD_SHARED=ON
cmake --build build --config Release
```

### Integrating into Your CMake Project

After building and installing, you can use SimpleScriptEngine in your own project:

```cmake
find_package(SimpleScriptEngine REQUIRED)
target_link_libraries(your_target PRIVATE SimpleScriptEngine::SimpleScriptEngine)
```

Or add it as a subdirectory:

```cmake
add_subdirectory(path/to/SimpleScriptEngine)
target_link_libraries(your_target PRIVATE SimpleScriptEngine)
```

---

## Usage

### Quick Example: Direct Engine Usage

```cpp
#include "SimpleScriptEngine.h"
#include "Engines/LuaJITEngine.h"

using namespace SimpleScriptEngine;

// Create and initialize an engine
auto engine = std::make_unique<LuaJITEngine>();
engine->initialize();

// Execute script code
engine->executeString("x = 42");

// Set global variables from C++
engine->setGlobal("message", SimpleScriptValue::Str("Hello from C++"));

// Call script functions from C++
engine->executeString("function add(a, b) return a + b end");
auto result = engine->call("add", {
    SimpleScriptValue::Int(3),
    SimpleScriptValue::Int(4)
});
std::cout << result.asInt() << std::endl;  // 7

// Register C++ functions for script use
engine->registerFunction("cpp_log", [](const std::vector<SimpleScriptValue>& args) {
    if (!args.empty()) std::cout << "[Script] " << args[0].toString() << std::endl;
    return SimpleScriptValue::Null();
});

// Evaluate expressions
auto val = engine->eval("10 * 5 + 3");
std::cout << val.asInt() << std::endl;  // 53

// Execute script files
engine->executeFile("scripts/main.lua");

// Check if a function exists
if (engine->hasFunction("add")) {
    auto sum = engine->call("add", { SimpleScriptValue::Int(10), SimpleScriptValue::Int(20) });
    std::cout << sum.asInt() << std::endl;  // 30
}

// Clean up
engine->shutdown();
```

### Using SimpleScriptManager with Smart Routing

```cpp
#include "SimpleScriptManager.h"
#include "Engines/LuaJITEngine.h"
#include "Engines/QuickJSEngine.h"

using namespace SimpleScriptEngine;

auto& mgr = SimpleScriptManager::instance();

// Register engines
mgr.registerEngine(std::make_unique<LuaJITEngine>());
mgr.registerEngine(std::make_unique<QuickJSEngine>());

// Batch initialize all registered engines
mgr.initializeAll();

// Execute by engine name
mgr.executeString("LuaJIT", "print('Hello Lua')");
mgr.executeString("QuickJS", "console.log('Hello JS')");

// Auto-route by file extension
mgr.executeFile("scripts/main.lua");   // → LuaJIT
mgr.executeFile("scripts/config.js");  // → QuickJS

// Cross-engine shared global variables
mgr.setSharedGlobal("app_name", SimpleScriptValue::Str("MyApp"));
auto name = mgr.getSharedGlobal("app_name");
std::cout << name.asString() << std::endl;  // "MyApp"

// List all registered engines
for (const auto& eng : mgr.listEngines()) {
    std::cout << "Engine: " << eng << std::endl;
}

// Look up engine by extension
auto* jsEngine = mgr.findEngineByExtension(".js");

// Clean up
mgr.shutdownAll();
```

### Script Module Import (Engine-Specific)

Each engine supports loading sub-module scripts using its native mechanism:

| Engine | Mechanism | Example |
|--------|-----------|---------|
| **LuaJIT** | `dofile()` | `local mod = dofile("module.lua")` |
| **QuickJS** | ES Modules | `import { fn } from "./module.js"` |
| **ChaiScript** | `use()` | `use("module.chai")` |
| **AngelScript** | `#include` | `#include "module.as"` |

### Error Callback

```cpp
engine->setErrorCallback([](const std::string& engineName, const std::string& message) {
    std::cerr << "[" << engineName << " Error] " << message << std::endl;
});
```

### Best Practices

1. **Always pair `initialize()` with `shutdown()`** — use RAII wrappers if possible
2. **Check `isInitialized()` before use** — especially after `initialize()` returns
3. **Check `hasFunction()` before `call()`** — avoid crashes on missing functions
4. **Set an error callback** — catch script errors early during development
5. **Prefer `SimpleScriptManager` for multi-engine projects** — it handles routing and lifecycle automatically
6. **Use `SimpleScriptValue` factory methods** (`Int()`, `Str()`, etc.) rather than constructors for clarity

---

## API Reference

### SimpleScriptValue Types

```cpp
enum class ScriptValueType { Null, Bool, Int, Double, String, Array, Object, Function };

// Type aliases
using ScriptFunction = std::function<SimpleScriptValue(const std::vector<SimpleScriptValue>&)>;
using ScriptObject   = std::unordered_map<std::string, SimpleScriptValue>;
using ScriptArray    = std::vector<SimpleScriptValue>;
```

### SimpleScriptEngine Interface (Abstract)

```cpp
class SimpleScriptEngine {
public:
    virtual ~SimpleScriptEngine() = default;

    // Metadata (always available — noexcept)
    virtual const char* engineName()    const noexcept = 0;  // e.g., "LuaJIT"
    virtual const char* engineVersion() const noexcept = 0;  // e.g., "2.1.0"
    virtual const char* fileExtension() const noexcept = 0;  // e.g., ".lua"

    // Lifecycle
    virtual bool initialize() = 0;
    virtual void shutdown()   = 0;
    virtual bool isInitialized() const noexcept = 0;

    // Execution
    virtual bool              executeString(const std::string& code) = 0;
    virtual bool              executeFile(const std::string& path)   = 0;
    virtual SimpleScriptValue eval(const std::string& expr)          = 0;

    // C++ → Script
    virtual bool              hasFunction(const std::string& name) = 0;
    virtual SimpleScriptValue call(const std::string& func,
                                   const std::vector<SimpleScriptValue>& args = {}) = 0;
    virtual void              setGlobal(const std::string& name, const SimpleScriptValue& value) = 0;
    virtual SimpleScriptValue getGlobal(const std::string& name) = 0;

    // Script → C++
    virtual void registerFunction(const std::string& name, ScriptFunction func) = 0;

    // Error handling
    using ErrorCallback = std::function<void(const std::string& engineName,
                                              const std::string& message)>;
    virtual void setErrorCallback(ErrorCallback cb);
};
```

### SimpleScriptManager (Singleton)

```cpp
class SimpleScriptManager {
public:
    static SimpleScriptManager& instance();

    // Engine registration
    bool registerEngine(std::unique_ptr<SimpleScriptEngine> engine);
    bool unregisterEngine(const std::string& name);
    SimpleScriptEngine* getEngine(const std::string& name);
    SimpleScriptEngine* findEngineByExtension(const std::string& ext);
    std::vector<std::string> listEngines() const;

    // Batch lifecycle
    size_t initializeAll();
    void   shutdownAll();

    // Smart routing
    bool executeString(const std::string& engineName, const std::string& code);
    bool executeFile(const std::string& path);
    bool hasFunction(const std::string& engineName, const std::string& funcName);
    SimpleScriptValue call(const std::string& engineName, const std::string& funcName,
                           const std::vector<SimpleScriptValue>& args = {});

    // Cross-engine shared globals
    void setSharedGlobal(const std::string& name, const SimpleScriptValue& value);
    SimpleScriptValue getSharedGlobal(const std::string& name);
    bool hasSharedGlobal(const std::string& name);
};
```

---

## CMake Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `SIMPLESCRIPTENGINE_ENABLE_LUAJIT` | `ON` | Enable LuaJIT engine |
| `SIMPLESCRIPTENGINE_ENABLE_QUICKJS` | `ON` | Enable QuickJS engine |
| `SIMPLESCRIPTENGINE_ENABLE_CHAISCRIPT` | `ON` | Enable ChaiScript engine |
| `SIMPLESCRIPTENGINE_ENABLE_ANGELSCRIPT` | `ON` | Enable AngelScript engine |
| `SIMPLESCRIPTENGINE_BUILD_EXAMPLES` | `ON` | Build example programs |
| `SIMPLESCRIPTENGINE_BUILD_SHARED` | `OFF` | Build as shared library (default: static) |
| `ENABLE_PROXY` | `OFF` | Enable proxy for downloading third-party dependencies |
| `DEFAULT_HTTP_PROXY` | (empty) | Default HTTP proxy URL |
| `DEFAULT_HTTPS_PROXY` | (empty) | Default HTTPS proxy URL |

---

## Platform Support

| Platform | Compiler | Status |
|----------|----------|--------|
| **Windows** | MSVC 2019+ | ✅ Supported |
| **Linux** | GCC 9+, Clang 10+ | ✅ Supported |
| **macOS** | Clang 10+ | ✅ Supported |

> **Note:** On Windows, the example program sets the console output code page to UTF-8 to handle international characters correctly.

---

## Third-Party Dependencies

All dependencies are downloaded automatically during the CMake configure step. No manual installation is required.

| Dependency | Source | Version | Purpose | License |
|------------|--------|---------|---------|---------|
| **luajit-rocks** | [torch/luajit-rocks](https://github.com/torch/luajit-rocks) | master | LuaJIT runtime | MIT |
| **quickjs-ng** | [quickjs-ng/quickjs](https://github.com/quickjs-ng/quickjs) | 0.15.1 | QuickJS runtime | MIT |
| **ChaiScript** | [ChaiScript/ChaiScript](https://github.com/ChaiScript/ChaiScript) | 6.1.0 | ChaiScript runtime (header-only) | BSD |
| **AngelScript** | [anjo76/angelscript](https://github.com/anjo76/angelscript) | 2.38.0 | AngelScript runtime | zlib |

Dependencies are extracted to the `3rdparty/` directory, with zip archives cached in `3rdparty-download/`.

---

## Offline Build

If your build machine does not have internet access:

1. Run CMake configure once on a machine with internet access to generate the `3rdparty/` and `3rdparty-download/` directories
2. Copy the entire project (including these directories) to the offline environment
3. Build normally — CMake will detect the cached dependencies and skip downloads

---

## Contributing

Contributions are welcome! Please feel free to submit issues or pull requests.

### Development Setup

```bash
git clone <repository-url>
cd SimpleScriptEngine
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DSIMPLESCRIPTENGINE_BUILD_EXAMPLES=ON
cmake --build build --config Debug
```

### Code Style

- C++17 standard with extensions disabled
- Header guards use `#pragma once`
- Namespace: `SimpleScriptEngine`
- Public API documented with Doxygen-style comments

---

## License

This project is licensed under the [MIT License](LICENSE).

Each integrated scripting engine retains its original license:

| Engine | License |
|--------|---------|
| LuaJIT | MIT |
| QuickJS | MIT |
| ChaiScript | BSD |
| AngelScript | zlib |

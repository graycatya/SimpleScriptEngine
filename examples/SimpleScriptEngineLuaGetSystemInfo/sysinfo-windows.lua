-- ============================================================================
-- LuaJIT FFI Windows System Information Collection Script
-- ============================================================================
-- Demonstrates calling C APIs in kernel32.dll / user32.dll / ntdll.dll via
-- LuaJIT FFI directly to collect CPU, memory, OS version, computer name,
-- screen resolution, environment variables, and more.
-- ============================================================================

local ffi = require("ffi")

-- Explicitly load non-default namespace DLLs
--   ntdll    : RtlGetVersion
--   advapi32 : GetUserNameA
local ntdll    = ffi.load("ntdll")
local advapi32 = ffi.load("advapi32")

-- ============================================================================
-- 1. Declare Windows API structs & functions
-- ============================================================================
ffi.cdef[[
    // ---- SYSTEM_INFO (kernel32.dll) ----
    typedef struct {
        uint32_t dwOemId;
        uint32_t dwPageSize;
        void*    lpMinimumApplicationAddress;
        void*    lpMaximumApplicationAddress;
        void*    dwActiveProcessorMask;
        uint32_t dwNumberOfProcessors;
        uint32_t dwProcessorType;
        uint32_t dwAllocationGranularity;
        uint16_t wProcessorLevel;
        uint16_t wProcessorRevision;
    } SYSTEM_INFO;
    void GetSystemInfo(SYSTEM_INFO* lpSystemInfo);

    // ---- GetNativeSystemInfo (64-bit awareness) ----
    void GetNativeSystemInfo(SYSTEM_INFO* lpSystemInfo);

    // ---- MEMORYSTATUSEX (kernel32.dll) ----
    typedef struct {
        uint32_t dwLength;
        uint32_t dwMemoryLoad;
        uint64_t ullTotalPhys;
        uint64_t ullAvailPhys;
        uint64_t ullTotalPageFile;
        uint64_t ullAvailPageFile;
        uint64_t ullTotalVirtual;
        uint64_t ullAvailVirtual;
        uint64_t ullAvailExtendedVirtual;
    } MEMORYSTATUSEX;
    int GlobalMemoryStatusEx(MEMORYSTATUSEX* lpBuffer);

    // ---- OSVERSIONINFOEX (ntdll.dll) ----
    typedef struct {
        uint32_t dwOSVersionInfoSize;
        uint32_t dwMajorVersion;
        uint32_t dwMinorVersion;
        uint32_t dwBuildNumber;
        uint32_t dwPlatformId;
        char     szCSDVersion[128];
        uint16_t wServicePackMajor;
        uint16_t wServicePackMinor;
        uint16_t wSuiteMask;
        uint8_t  wProductType;
        uint8_t  wReserved;
    } OSVERSIONINFOEXA;
    int RtlGetVersion(OSVERSIONINFOEXA* lpVersionInformation);

    // ---- Computer name / user name (kernel32.dll / advapi32.dll) ----
    typedef uint32_t DWORD;
    typedef DWORD*    LPDWORD;
    int GetComputerNameA(char* lpBuffer, LPDWORD nSize);
    int GetUserNameA(char* lpBuffer, LPDWORD nSize);

    // ---- System directories (kernel32.dll) ----
    uint32_t GetSystemDirectoryA(char* lpBuffer, uint32_t uSize);
    uint32_t GetWindowsDirectoryA(char* lpBuffer, uint32_t uSize);

    // ---- Screen resolution (user32.dll) ----
    int GetSystemMetrics(int nIndex);

    // ---- Keyboard layout / locale (user32.dll / kernel32.dll) ----
    uint32_t GetKeyboardLayout(uint32_t idThread);
    int GetLocaleInfoA(uint32_t Locale, uint32_t LCType, char* lpLCData, int cchData);

    // ---- Power status (kernel32.dll) ----
    typedef struct {
        uint8_t ACLineStatus;
        uint8_t BatteryFlag;
        uint8_t BatteryLifePercent;
        uint8_t Reserved1;
        uint32_t BatteryLifeTime;
        uint32_t BatteryFullLifeTime;
    } SYSTEM_POWER_STATUS;
    int GetSystemPowerStatus(SYSTEM_POWER_STATUS* lpSystemPowerStatus);

    // ---- System uptime (kernel32.dll) ----
    uint64_t GetTickCount64(void);
]]

-- ============================================================================
-- Helper Functions
-- ============================================================================

local function fmt_bytes(bytes)
    if bytes < 1024 then
        return string.format("%d B", bytes)
    elseif bytes < 1024 * 1024 then
        return string.format("%.2f KB", bytes / 1024)
    elseif bytes < 1024 * 1024 * 1024 then
        return string.format("%.2f MB", bytes / (1024 ^ 2))
    else
        return string.format("%.2f GB", bytes / (1024 ^ 3))
    end
end

-- Read a null-terminated string from a char buffer
local function buf_to_string(buf)
    return ffi.string(buf)
end

-- Call a Win32 API that returns a string (buf + output size pointer pattern)
local function win32_get_string(get_fn)
    local buf = ffi.new("char[512]")
    local psize = ffi.new("DWORD[1]")
    psize[0] = 512
    if get_fn(buf, psize) ~= 0 then
        return ffi.string(buf, psize[0])
    end
    return "N/A"
end

-- Call a Win32 API that returns a string (buf + input size value pattern)
local function win32_get_string_len(get_fn)
    local buf = ffi.new("char[512]")
    local len = get_fn(buf, 512)
    if len > 0 then
        return ffi.string(buf, len)
    end
    return "N/A"
end

-- ============================================================================
-- 2. CPU Info
-- ============================================================================
bit = require("bit")

local function get_cpu_info()
    local info = ffi.new("SYSTEM_INFO")
    ffi.C.GetNativeSystemInfo(info)

    -- wProcessorArchitecture is in the low 16 bits of dwOemId (union)
    --   0 = x86, 5 = ARM, 6 = IA64, 9 = x64/AMD64, 12 = ARM64
    local arch_id = bit.band(tonumber(info.dwOemId), 0xFFFF)
    local arch_map = {
        [0]  = "x86",
        [5]  = "ARM",
        [6]  = "IA64",
        [9]  = "x64 (AMD64)",
        [12] = "ARM64"
    }
    local arch = arch_map[arch_id] or ("Unknown (" .. arch_id .. ")")

    return {
        processors           = tonumber(info.dwNumberOfProcessors),
        page_size_kb         = string.format("%.0f", info.dwPageSize / 1024),
        alloc_granularity_kb = string.format("%.0f", info.dwAllocationGranularity / 1024),
        arch                 = arch,
        arch_id              = arch_id
    }
end

-- ============================================================================
-- 3. Memory Info
-- ============================================================================
local function get_memory_info()
    local mem = ffi.new("MEMORYSTATUSEX")
    mem.dwLength = ffi.sizeof("MEMORYSTATUSEX")
    ffi.C.GlobalMemoryStatusEx(mem)

    return {
        total_phys    = fmt_bytes(tonumber(mem.ullTotalPhys / 1)),
        avail_phys    = fmt_bytes(tonumber(mem.ullAvailPhys / 1)),
        load_percent  = tonumber(mem.dwMemoryLoad),
        total_virtual = fmt_bytes(tonumber(mem.ullTotalVirtual / 1)),
        avail_virtual = fmt_bytes(tonumber(mem.ullAvailVirtual / 1)),
    }
end

-- ============================================================================
-- 4. OS Version
-- ============================================================================
local function get_os_version()
    local ver = ffi.new("OSVERSIONINFOEXA")
    ver.dwOSVersionInfoSize = ffi.sizeof("OSVERSIONINFOEXA")
    ntdll.RtlGetVersion(ver)

    local product_map = {
        [1] = "Workstation",
        [2] = "Domain Controller",
        [3] = "Server"
    }

    return {
        major     = tonumber(ver.dwMajorVersion),
        minor     = tonumber(ver.dwMinorVersion),
        build     = tonumber(ver.dwBuildNumber),
        platform  = tonumber(ver.dwPlatformId),
        product   = product_map[ver.wProductType] or ("Type " .. ver.wProductType),
        sp_major  = tonumber(ver.wServicePackMajor),
        sp_minor  = tonumber(ver.wServicePackMinor),
    }
end

-- ============================================================================
-- 5. Computer Name / User Name / Directories
-- ============================================================================
local function get_system_names()
    return {
        computer    = win32_get_string(ffi.C.GetComputerNameA),
        user        = win32_get_string(advapi32.GetUserNameA),
        windows_dir = win32_get_string_len(ffi.C.GetWindowsDirectoryA),
        system_dir  = win32_get_string_len(ffi.C.GetSystemDirectoryA),
    }
end

-- ============================================================================
-- 6. Screen Resolution
-- ============================================================================
local function get_screen_info()
    local SM_CXSCREEN = 0
    local SM_CYSCREEN = 1
    local SM_CXFULLSCREEN = 16
    local SM_CYFULLSCREEN = 17

    return {
        primary_width  = ffi.C.GetSystemMetrics(SM_CXSCREEN),
        primary_height = ffi.C.GetSystemMetrics(SM_CYSCREEN),
    }
end

-- ============================================================================
-- 7. System Uptime
-- ============================================================================
local function get_uptime()
    local ms = tonumber(ffi.C.GetTickCount64())
    local seconds = math.floor(ms / 1000)
    local days    = math.floor(seconds / 86400)
    local hours   = math.floor((seconds % 86400) / 3600)
    local minutes = math.floor((seconds % 3600) / 60)
    local secs    = seconds % 60

    return string.format("%d days %02d:%02d:%02d", days, hours, minutes, secs)
end

-- ============================================================================
-- 8. Battery Status
-- ============================================================================
local function get_power_info()
    local ps = ffi.new("SYSTEM_POWER_STATUS")
    local ok = ffi.C.GetSystemPowerStatus(ps)
    if ok == 0 then
        return { battery = "N/A (desktop or no battery)" }
    end

    local ac_map = { [0] = "On Battery", [1] = "AC Power", [255] = "Unknown" }

    return {
        ac_status  = ac_map[ps.ACLineStatus] or "Unknown",
        battery    = ps.BatteryFlag ~= 128 and (ps.BatteryLifePercent .. "%") or "No Battery",
    }
end

-- ============================================================================
-- 9. Environment Variables
-- ============================================================================
local function get_env()
    return {
        COMPUTERNAME = os.getenv("COMPUTERNAME") or "N/A",
        USERNAME     = os.getenv("USERNAME")     or "N/A",
        TEMP         = os.getenv("TEMP")         or "N/A",
        USERPROFILE  = os.getenv("USERPROFILE")  or "N/A",
        OS           = os.getenv("OS")           or "N/A",
        PROCESSOR_ARCHITECTURE = os.getenv("PROCESSOR_ARCHITECTURE") or "N/A",
        NUMBER_OF_PROCESSORS   = os.getenv("NUMBER_OF_PROCESSORS")   or "N/A",
    }
end

-- ============================================================================
-- Print All Info
-- ============================================================================
local function print_separator(title)
    print(string.rep("=", 60))
    print("  " .. title)
    print(string.rep("=", 60))
end

local function print_kv(key, value, indent)
    local pad = indent and string.rep(" ", indent) or "  "
    print(string.format("%s%-24s : %s", pad, key, value))
end

local function run()
    print_separator("LuaJIT FFI -- Windows System Information")
    print()

    -- CPU
    local cpu = get_cpu_info()
    print_separator("CPU Info")
    print_kv("Logical Processors",  cpu.processors)
    print_kv("Processor Arch",      cpu.arch)
    print_kv("Page Size",           cpu.page_size_kb .. " KB")
    print_kv("Alloc Granularity",   cpu.alloc_granularity_kb .. " KB")
    print()

    -- Memory
    local mem = get_memory_info()
    print_separator("Memory Info")
    print_kv("Total Physical",    mem.total_phys)
    print_kv("Available Physical", mem.avail_phys)
    print_kv("Memory Load",       mem.load_percent .. "%")
    print_kv("Total Virtual",     mem.total_virtual)
    print_kv("Available Virtual", mem.avail_virtual)
    print()

    -- OS Version
    local ver = get_os_version()
    print_separator("OS Version")
    print_kv("Version",       string.format("%d.%d (Build %d)", ver.major, ver.minor, ver.build))
    print_kv("Product Type",  ver.product)
    print_kv("Service Pack",  ver.sp_major .. "." .. ver.sp_minor)
    print()

    -- Computer Name / User Name / Directories
    local names = get_system_names()
    print_separator("System Info")
    print_kv("Computer Name",  names.computer)
    print_kv("Current User",   names.user)
    print_kv("Windows Dir",    names.windows_dir)
    print_kv("System Dir",     names.system_dir)
    print()

    -- Display
    local screen = get_screen_info()
    print_separator("Display")
    print_kv("Primary Resolution", screen.primary_width .. " x " .. screen.primary_height)
    print()

    -- Uptime
    print_separator("Uptime")
    print_kv("Up Time", get_uptime())
    print()

    -- Battery
    local power = get_power_info()
    print_separator("Power Status")
    print_kv("Power Source",  power.ac_status)
    print_kv("Battery Level", power.battery)
    print()

    -- Environment Variables
    local env = get_env()
    print_separator("Environment Variables")
    for k, v in pairs(env) do
        print_kv(k, v)
    end
    print()

    print_separator("Done")
end

run()

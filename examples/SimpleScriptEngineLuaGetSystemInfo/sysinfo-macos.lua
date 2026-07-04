-- ============================================================================
-- LuaJIT FFI macOS System Information Collection Script
-- ============================================================================
-- All data is collected exclusively through LuaJIT FFI calls to native macOS
-- libraries, without spawning external processes:
--
--   Framework          |  Purpose
--   -------------------+-----------------------------------------------
--   libSystem (ffi.C)  |  sysctl, mach_host_statistics64, statfs,
--                       |  gethostname, getlogin_r, sysconf, getenv
--   CoreGraphics       |  CGMainDisplayID / CGDisplayPixelsWide/High
--   IOKit              |  IOPowerSources (battery / AC status)
--   CoreFoundation     |  CFDictionaryGetValue / CFNumberGetValue / ...
-- ============================================================================

local ffi = require("ffi")

-- ---------------------------------------------------------------------------
-- Load macOS frameworks
-- ---------------------------------------------------------------------------
-- Use full framework paths so LuaJIT does not mangle the name to
-- "libCoreGraphics.dylib".  On modern macOS the dylibs live in the dyld
-- shared cache, but dlopen() on these paths still works via dyld.
local CoreGraphics    = ffi.load("/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics")
local IOKit           = ffi.load("/System/Library/Frameworks/IOKit.framework/IOKit")
local CoreFoundation  = ffi.load("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation")

-- ============================================================================
-- FFI C Declarations
-- ============================================================================

ffi.cdef[[
    // ========== sysctl (libSystem) ==========
    int  sysctlbyname(const char *name, void *oldp, size_t *oldlenp,
                      void *newp, size_t newlen);
    int  sysctl(int *name, unsigned int namelen, void *oldp, size_t *oldlenp,
                void *newp, size_t newlen);

    // ========== sysconf (libSystem) ==========
    long sysconf(int name);

    // ========== uname (libSystem) ==========
    struct utsname {
        char sysname[256];
        char nodename[256];
        char release[256];
        char version[256];
        char machine[256];
    };
    int  uname(struct utsname *buf);

    // ========== hostname / username (libSystem) ==========
    int  gethostname(char *name, size_t namelen);
    int  getlogin_r(char *buf, size_t buflen);

    // ========== getenv (libSystem) ==========
    char *getenv(const char *name);

    // ========== uptime via sysctl KERN_BOOTTIME (libSystem) ==========
    typedef long time_t;
    struct timespec {
        time_t tv_sec;
        long   tv_nsec;
    };

    // ========== Mach host_statistics64 for memory (libSystem) ==========
    typedef unsigned int        mach_port_t;
    typedef int                 kern_return_t;
    typedef int                 integer_t;
    typedef unsigned int        natural_t;
    typedef natural_t           mach_msg_type_number_t;

    struct vm_statistics64 {
        natural_t  free_count;
        natural_t  active_count;
        natural_t  inactive_count;
        natural_t  wire_count;
        uint64_t   zero_fill_count;
        uint64_t   reactivations;
        uint64_t   pageins;
        uint64_t   pageouts;
        uint64_t   faults;
        uint64_t   cow_faults;
        uint64_t   lookups;
        uint64_t   hits;
        uint64_t   purges;
        natural_t  purgeable_count;
        natural_t  speculative_count;
        uint64_t   decompressions;
        uint64_t   compressions;
        uint64_t   swapins;
        uint64_t   swapouts;
        natural_t  compressor_page_count;
        natural_t  throttled_count;
        natural_t  external_page_count;
        natural_t  internal_page_count;
        uint64_t   total_uncompressed_pages_in_compressor;
    };

    mach_port_t      mach_host_self(void);
    kern_return_t    host_statistics64(mach_port_t host_priv, int flavor,
                                       struct vm_statistics64 *info,
                                       mach_msg_type_number_t *count);

    // ========== statfs for disk info (libSystem) ==========
    struct statfs {
        uint32_t  f_bsize;
        int32_t   f_iosize;
        uint64_t  f_blocks;
        uint64_t  f_bfree;
        uint64_t  f_bavail;
        uint64_t  f_files;
        uint64_t  f_ffree;
        uint32_t  f_fsid[2];
        uint32_t  f_owner;
        uint32_t  f_type;
        uint32_t  f_flags;
        uint32_t  f_fssubtype;
        char      f_fstypename[16];
        char      f_mntonname[1024];
        char      f_mntfromname[1024];
        uint32_t  f_flags_ext;
        uint32_t  f_reserved[7];
    };
    int  statfs(const char *path, struct statfs *buf);

    // ========== CoreGraphics display (CoreGraphics.framework) ==========
    typedef uint32_t CGDirectDisplayID;
    CGDirectDisplayID CGMainDisplayID(void);
    size_t            CGDisplayPixelsWide(CGDirectDisplayID display);
    size_t            CGDisplayPixelsHigh(CGDirectDisplayID display);

    // ========== CoreFoundation types (CoreFoundation.framework) ==========
    typedef const void * CFTypeRef;
    typedef CFTypeRef    CFArrayRef;
    typedef CFTypeRef    CFDictionaryRef;
    typedef CFTypeRef    CFStringRef;
    typedef CFTypeRef    CFBooleanRef;
    typedef CFTypeRef    CFNumberRef;
    typedef long         CFIndex;
    typedef uint32_t     CFStringEncoding;
    typedef uint32_t     CFOptionFlags;
    typedef int32_t      CFNumberType;
    typedef void *       CFAllocatorRef;

    // ========== CoreFoundation functions ==========
    CFIndex      CFArrayGetCount(CFArrayRef theArray);
    CFTypeRef    CFArrayGetValueAtIndex(CFArrayRef theArray, CFIndex idx);
    CFTypeRef    CFDictionaryGetValue(CFDictionaryRef theDict, const void *key);
    bool         CFNumberGetValue(CFNumberRef number, CFNumberType theType,
                                  void *valuePtr);
    bool         CFBooleanGetValue(CFBooleanRef boolean);
    bool         CFStringGetCString(CFStringRef theString, char *buffer,
                                    CFIndex bufferSize, CFStringEncoding encoding);
    CFStringRef  CFStringCreateWithCString(CFAllocatorRef alloc,
                                           const char *cStr,
                                           CFStringEncoding encoding);
    void         CFRelease(CFTypeRef cf);

    // ========== IOKit Power Sources (IOKit.framework) ==========
    typedef mach_port_t io_object_t;
    CFTypeRef    IOPSCopyPowerSourcesInfo(void);
    CFArrayRef   IOPSCopyPowerSourcesList(CFTypeRef blob);
    CFDictionaryRef IOPSGetPowerSourceDescription(CFTypeRef blob, CFTypeRef ps);
]]

-- ============================================================================
-- Helpers
-- ============================================================================

-- CoreFoundation constants
local kCFStringEncodingUTF8 = 0x08000100
local kCFNumberIntType      = 3   -- kCFNumberSInt32Type

-- Mach constants
local HOST_VM_INFO64        = 4
local HOST_VM_INFO64_COUNT  = ffi.sizeof("struct vm_statistics64") / ffi.sizeof("integer_t")

-- Make a CFString key for dictionary lookup
local function cfstr(s)
    local cf = CoreFoundation.CFStringCreateWithCString(nil, s, kCFStringEncodingUTF8)
    return cf
end

-- Read CFNumber as int32
local function cfnumber_as_int(cfnum)
    if cfnum == nil then return nil end
    local val = ffi.new("int32_t[1]")
    if CoreFoundation.CFNumberGetValue(cfnum, kCFNumberIntType, val) then
        return tonumber(val[0])
    end
    return nil
end

-- Read CFString as Lua string
local function cfstring_as_lua(cfstr_ref)
    if cfstr_ref == nil then return nil end
    local buf = ffi.new("char[512]")
    if CoreFoundation.CFStringGetCString(cfstr_ref, buf, 512, kCFStringEncodingUTF8) then
        return ffi.string(buf)
    end
    return nil
end

-- Read CFBoolean as Lua boolean
local function cfbool(cfb)
    if cfb == nil then return nil end
    return CoreFoundation.CFBooleanGetValue(cfb)
end

-- sysctl string by name
local function sysctl_str(name)
    local buf = ffi.new("char[1024]")
    local len = ffi.new("size_t[1]", 1024)
    if ffi.C.sysctlbyname(name, buf, len, nil, 0) == 0 then
        return ffi.string(buf, len[0] - 1)
    end
    return "N/A"
end

-- sysctl int64 by name
local function sysctl_int64(name)
    local val = ffi.new("int64_t[1]")
    local len = ffi.new("size_t[1]", 8)
    if ffi.C.sysctlbyname(name, val, len, nil, 0) == 0 then
        return tonumber(val[0])
    end
    return nil
end

-- sysctl int32 by name
local function sysctl_int32(name)
    local val = ffi.new("int32_t[1]")
    local len = ffi.new("size_t[1]", 4)
    if ffi.C.sysctlbyname(name, val, len, nil, 0) == 0 then
        return tonumber(val[0])
    end
    return nil
end

-- Human-readable byte size
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

-- ============================================================================
-- 1. CPU Info  (sysctl via ffi.C)
-- ============================================================================
local function get_cpu_info()
    local brand    = sysctl_str("machdep.cpu.brand_string")
    local arch     = sysctl_str("hw.machine")
    local logical  = sysctl_int32("hw.logicalcpu") or 0
    local physical = sysctl_int32("hw.physicalcpu") or 0
    local freq_hz  = sysctl_int64("hw.cpufrequency")
    local freq     = freq_hz and string.format("%.1f GHz", freq_hz / 1e9) or "N/A"
    local l3       = sysctl_int64("hw.l3cachesize")
    local pagesize = sysctl_int64("hw.pagesize")
    local byteorder = sysctl_int32("hw.byteorder") or 0
    local bo_str = (byteorder == 1234) and "Little Endian" or "Big Endian"

    return {
        brand          = brand,
        arch           = arch,
        logical_cores  = logical,
        physical_cores = physical,
        frequency      = freq,
        l3_cache       = l3 and fmt_bytes(tonumber(l3)) or "N/A",
        page_size      = pagesize and string.format("%.0f KB", pagesize / 1024) or "N/A",
        byte_order     = bo_str,
    }
end

-- ============================================================================
-- 2. Memory Info  (Mach host_statistics64 via ffi.C / libSystem)
-- ============================================================================
local function get_memory_info()
    local mem_total = sysctl_int64("hw.memsize") or 0
    local pagesize  = sysctl_int64("hw.pagesize") or 4096

    local host = ffi.C.mach_host_self()
    local vm   = ffi.new("struct vm_statistics64")
    local count = ffi.new("mach_msg_type_number_t[1]", HOST_VM_INFO64_COUNT)

    local kr = ffi.C.host_statistics64(host, HOST_VM_INFO64, vm, count)
    if kr ~= 0 then
        return {
            total        = fmt_bytes(mem_total),
            used         = "N/A",
            available    = "N/A",
            load_percent = "N/A",
        }
    end

    local free_count       = tonumber(vm.free_count)
    local active_count     = tonumber(vm.active_count)
    local inactive_count   = tonumber(vm.inactive_count)
    local wire_count       = tonumber(vm.wire_count)
    local compressed_count = tonumber(vm.compressor_page_count)
    local speculative      = tonumber(vm.speculative_count)

    local free_bytes       = free_count       * pagesize
    local active_bytes     = active_count     * pagesize
    local inactive_bytes   = inactive_count   * pagesize
    local wire_bytes       = wire_count       * pagesize
    local compressed_bytes = compressed_count * pagesize

    local used_bytes  = active_bytes + wire_bytes + compressed_bytes
    local avail_bytes = free_bytes + inactive_bytes

    return {
        total        = fmt_bytes(mem_total),
        used         = fmt_bytes(used_bytes),
        available    = fmt_bytes(avail_bytes),
        load_percent = mem_total > 0 and math.floor(used_bytes / mem_total * 100 + 0.5) or 0,
        free         = fmt_bytes(free_bytes),
        active       = fmt_bytes(active_bytes),
        inactive     = fmt_bytes(inactive_bytes),
        wired        = fmt_bytes(wire_bytes),
        compressed   = fmt_bytes(compressed_bytes),
        speculative  = fmt_bytes(speculative * pagesize),
        swapins      = tonumber(vm.swapins),
        swapouts     = tonumber(vm.swapouts),
    }
end

-- ============================================================================
-- 3. OS Version  (sysctl via ffi.C)
-- ============================================================================
local function get_os_version()
    local ver     = sysctl_str("kern.osproductversion")
    local build   = sysctl_str("kern.osversion")
    local release = sysctl_str("kern.osrelease")
    local ostype  = sysctl_str("kern.ostype")
    local bootargs = sysctl_str("kern.bootargs")

    return {
        version  = ver,
        build    = build,
        release  = release,
        ostype   = ostype,
        bootargs = (bootargs ~= "" and bootargs ~= "N/A") and bootargs or "(none)",
    }
end

-- ============================================================================
-- 4. Hostname / Username  (POSIX via ffi.C)
-- ============================================================================
local function get_system_names()
    local buf = ffi.new("char[256]")
    ffi.C.gethostname(buf, 256)
    local hostname = ffi.string(buf)

    local ubuf = ffi.new("char[256]")
    local ret  = ffi.C.getlogin_r(ubuf, 256)
    local username = (ret == 0) and ffi.string(ubuf) or "N/A"

    return {
        hostname = hostname,
        username = username,
    }
end

-- ============================================================================
-- 5. System Uptime  (sysctl KERN_BOOTTIME via ffi.C)
-- ============================================================================
local function get_uptime()
    local mib = ffi.new("int[2]", { 1, 21 })  -- CTL_KERN=1, KERN_BOOTTIME=21
    local bt  = ffi.new("struct timespec")
    local sz  = ffi.new("size_t[1]", ffi.sizeof("struct timespec"))

    if ffi.C.sysctl(mib, 2, bt, sz, nil, 0) == 0 then
        local boot_sec = tonumber(bt.tv_sec)
        local diff     = math.floor(os.difftime(os.time(), boot_sec))
        if diff < 0 then diff = 0 end

        local days    = math.floor(diff / 86400)
        local hours   = math.floor((diff % 86400) / 3600)
        local minutes = math.floor((diff % 3600) / 60)
        local secs    = diff % 60
        return string.format("%d days %02d:%02d:%02d", days, hours, minutes, secs)
    end
    return "N/A"
end

-- ============================================================================
-- 6. Screen Resolution  (CoreGraphics.framework)
-- ============================================================================
local function get_screen_info()
    local display = CoreGraphics.CGMainDisplayID()
    if display == 0 then
        return { resolution = "N/A" }
    end

    local w = tonumber(CoreGraphics.CGDisplayPixelsWide(display))
    local h = tonumber(CoreGraphics.CGDisplayPixelsHigh(display))

    return {
        display_id  = tonumber(display),
        width       = w,
        height      = h,
        resolution  = string.format("%d x %d", w, h),
    }
end

-- ============================================================================
-- 7. Battery / Power  (IOKit.framework IOPowerSources + CoreFoundation)
-- ============================================================================
local function get_power_info()
    local info = IOKit.IOPSCopyPowerSourcesInfo()
    if info == nil then
        return { source = "N/A (desktop / unknown)", battery = "N/A" }
    end

    local list = IOKit.IOPSCopyPowerSourcesList(info)
    if list == nil then
        CoreFoundation.CFRelease(info)
        return { source = "N/A (desktop / unknown)", battery = "N/A" }
    end

    local count = tonumber(CoreFoundation.CFArrayGetCount(list))
    if count == 0 then
        CoreFoundation.CFRelease(list)
        CoreFoundation.CFRelease(info)
        return { source = "AC Power (no battery)", battery = "N/A" }
    end

    -- Read the first power source (most systems have one battery)
    local ps = CoreFoundation.CFArrayGetValueAtIndex(list, 0)
    local desc = IOKit.IOPSGetPowerSourceDescription(info, ps)

    if desc == nil then
        CoreFoundation.CFRelease(list)
        CoreFoundation.CFRelease(info)
        return { source = "N/A", battery = "N/A" }
    end

    -- Extract values from the CFDictionary
    local function cf_dict_str(dict, key)
        return cfstring_as_lua(CoreFoundation.CFDictionaryGetValue(dict, cfstr(key)))
    end
    local function cf_dict_int(dict, key)
        return cfnumber_as_int(CoreFoundation.CFDictionaryGetValue(dict, cfstr(key)))
    end
    local function cf_dict_bool(dict, key)
        return cfbool(CoreFoundation.CFDictionaryGetValue(dict, cfstr(key)))
    end

    -- Power source type: "InternalBattery", "AC Power", etc.
    local ps_type    = cf_dict_str(desc, "Type") or "Unknown"
    local ps_state   = cf_dict_str(desc, "Power Source State") or ""
    local charged    = cf_dict_bool(desc, "Is Charged")
    local charging   = cf_dict_bool(desc, "Is Charging")
    local pct        = cf_dict_int(desc, "Current Capacity")
    local max_pct    = cf_dict_int(desc, "Max Capacity")
    local present    = cf_dict_bool(desc, "Is Present")

    -- Parse power source (AC vs Battery)
    local source = "Unknown"
    if ps_state == "AC Power" then
        if charged then
            source = "AC Power (Charged)"
        elseif charging then
            source = "AC Power (Charging)"
        else
            source = "AC Power"
        end
    elseif ps_state == "Battery Power" then
        source = "Battery Power"
    end

    -- Battery percentage
    local batt_str = "N/A"
    if pct and max_pct and max_pct > 0 then
        batt_str = string.format("%.0f%%", pct / max_pct * 100)
    end

    -- Cleanup
    CoreFoundation.CFRelease(desc)
    CoreFoundation.CFRelease(list)
    CoreFoundation.CFRelease(info)

    return {
        source  = source,
        battery = batt_str,
        type    = ps_type,
    }
end

-- ============================================================================
-- 8. Disk Info  (statfs via ffi.C / libSystem)
-- ============================================================================
local function get_disk_info()
    local st = ffi.new("struct statfs")
    if ffi.C.statfs("/", st) ~= 0 then
        return {}
    end

    local bsize  = tonumber(st.f_bsize)
    local blocks = tonumber(st.f_blocks)
    local bfree  = tonumber(st.f_bfree)
    local bavail = tonumber(st.f_bavail)
    local files  = tonumber(st.f_files)
    local ffree  = tonumber(st.f_ffree)

    local total_bytes = blocks * bsize
    local free_bytes  = bavail * bsize
    local used_bytes  = total_bytes - free_bytes
    local use_pct     = total_bytes > 0 and math.floor(used_bytes / total_bytes * 100 + 0.5) or 0

    return {
        mount_point = ffi.string(st.f_mntonname),
        fs_type     = ffi.string(st.f_fstypename),
        total       = fmt_bytes(total_bytes),
        used        = fmt_bytes(used_bytes),
        available   = fmt_bytes(free_bytes),
        use_pct     = use_pct .. "%",
        total_files = files,
        free_files  = ffree,
    }
end

-- ============================================================================
-- 9. Environment Variables  (getenv via ffi.C)
-- ============================================================================
local function get_env()
    local function e(key)
        local p = ffi.C.getenv(key)
        return (p ~= nil) and ffi.string(p) or "N/A"
    end

    return {
        HOME   = e("HOME"),
        USER   = e("USER"),
        SHELL  = e("SHELL"),
        TMPDIR = e("TMPDIR"),
        LANG   = e("LANG"),
        PATH   = e("PATH"),
    }
end

-- ============================================================================
-- Print Helpers
-- ============================================================================
local function print_separator(title)
    print(string.rep("=", 60))
    print("  " .. title)
    print(string.rep("=", 60))
end

local function print_kv(key, value, indent)
    local pad = indent and string.rep(" ", indent) or "  "
    print(string.format("%s%-24s : %s", pad, key, tostring(value)))
end

-- ============================================================================
-- Run
-- ============================================================================
local function run()
    print_separator("LuaJIT FFI — macOS System Information")
    print()
    print("  Libraries loaded: libSystem (ffi.C)  CoreGraphics  IOKit  CoreFoundation")
    print()

    -- 1. CPU
    local cpu = get_cpu_info()
    print_separator("CPU Info")
    print_kv("Processor",        cpu.brand)
    print_kv("Architecture",     cpu.arch)
    print_kv("Logical Cores",    cpu.logical_cores)
    print_kv("Physical Cores",   cpu.physical_cores)
    print_kv("Frequency",        cpu.frequency)
    print_kv("L3 Cache",         cpu.l3_cache)
    print_kv("Page Size",        cpu.page_size)
    print_kv("Byte Order",       cpu.byte_order)
    print()

    -- 2. Memory (Mach host_statistics64)
    local mem = get_memory_info()
    print_separator("Memory Info  [Mach host_statistics64]")
    print_kv("Total Physical",   mem.total)
    print_kv("Used",             mem.used)
    print_kv("Available",        mem.available)
    print_kv("Memory Load",      tostring(mem.load_percent) .. "%")
    print_kv("Wired",            mem.wired)
    print_kv("Active",           mem.active)
    print_kv("Inactive",         mem.inactive)
    print_kv("Free",             mem.free)
    print_kv("Compressed",       mem.compressed)
    print_kv("Swap In (pages)",  mem.swapins)
    print_kv("Swap Out (pages)", mem.swapouts)
    print()

    -- 3. OS
    local osv = get_os_version()
    print_separator("OS Version")
    print_kv("System",           osv.ostype .. " (macOS)")
    print_kv("Version",          osv.version)
    print_kv("Build",            osv.build)
    print_kv("Kernel Release",   osv.release)
    print_kv("Boot Args",        osv.bootargs)
    print()

    -- 4. Host / User
    local names = get_system_names()
    print_separator("System Identity")
    print_kv("Hostname",         names.hostname)
    print_kv("Current User",     names.username)
    print()

    -- 5. Display (CoreGraphics)
    local screen = get_screen_info()
    print_separator("Display  [CoreGraphics.framework]")
    print_kv("Main Display ID",  screen.display_id or "N/A")
    print_kv("Resolution",       screen.resolution)
    print()

    -- 6. Uptime
    print_separator("Uptime")
    print_kv("Up Time",          get_uptime())
    print()

    -- 7. Battery (IOKit IOPowerSources)
    local power = get_power_info()
    print_separator("Power Status  [IOKit.framework]")
    print_kv("Type",             power.type or "N/A")
    print_kv("Power Source",     power.source)
    print_kv("Battery Level",    power.battery)
    print()

    -- 8. Disk (statfs)
    local disk = get_disk_info()
    if disk.total then
        print_separator("Root Disk  [statfs /]")
        print_kv("Mount Point",      disk.mount_point)
        print_kv("Filesystem",       disk.fs_type)
        print_kv("Total",            disk.total)
        print_kv("Used",             disk.used)
        print_kv("Available",        disk.available)
        print_kv("Usage",            disk.use_pct)
        print()
    end

    -- 9. Environment
    local env = get_env()
    print_separator("Environment Variables")
    for k, v in pairs(env) do
        print_kv(k, v)
    end
    print()

    print_separator("Done")
end

run()

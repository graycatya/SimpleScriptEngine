-- ============================================================================
-- LuaJIT FFI Linux System Information Collection Script
-- ============================================================================
-- All data is collected exclusively through LuaJIT FFI calls to native Linux
-- libraries, without spawning external processes:
--
--   Library        |  Purpose
--   ---------------+-------------------------------------------------
--   ffi.C (libc)   |  sysconf, sysinfo, uname, gethostname,
--                  |  getlogin_r, getenv, statfs, fopen/fgets/fclose
--   libX11         |  XOpenDisplay / DisplayWidth / DisplayHeight
--   /proc          |  cpuinfo (CPU model name — no pure FFI equivalent)
--   /sys           |  power_supply, cpufreq (battery / frequency)
-- ============================================================================

local ffi = require("ffi")

-- ---------------------------------------------------------------------------
-- Load Linux libraries
-- ---------------------------------------------------------------------------
-- libc is auto-loaded as ffi.C
local ok, x11 = pcall(function() return ffi.load("libX11.so.6") end)
local libX11 = ok and x11 or nil

-- ============================================================================
-- FFI C Declarations
-- ============================================================================

ffi.cdef[[
    // ========== POSIX / libc ==========
    int  uname(struct utsname *buf);

    struct utsname {
        char sysname[65];
        char nodename[65];
        char release[65];
        char version[65];
        char machine[65];
        char domainname[65];
    };

    struct sysinfo {
        long uptime;
        unsigned long loads[3];
        unsigned long totalram;
        unsigned long freeram;
        unsigned long sharedram;
        unsigned long bufferram;
        unsigned long totalswap;
        unsigned long freeswap;
        unsigned short procs;
        unsigned long totalhigh;
        unsigned long freehigh;
        unsigned int  mem_unit;
        char _f[20-2*sizeof(long)-sizeof(int)];
    };
    int sysinfo(struct sysinfo *info);

    int  gethostname(char *name, size_t namelen);
    int  getlogin_r(char *buf, size_t buflen);
    char *getenv(const char *name);
    long sysconf(int name);

    // ========== statfs for disk info ==========
    struct statfs {
        long    f_type;
        long    f_bsize;
        unsigned long f_blocks;
        unsigned long f_bfree;
        unsigned long f_bavail;
        unsigned long f_files;
        unsigned long f_ffree;
        int     f_fsid[2];
        long    f_namelen;
        long    f_frsize;
        long    f_flags;
        long    f_spare[4];
    };
    int  statfs(const char *path, struct statfs *buf);

    // ========== FILE I/O for reading /proc and /sys ==========
    typedef struct FILE FILE;
    FILE *fopen(const char *pathname, const char *mode);
    char *fgets(char *s, int size, FILE *stream);
    int   fclose(FILE *stream);
]]

if libX11 then
    ffi.cdef[[
        // ========== X11 display (libX11.so.6) ==========
        typedef struct _XDisplay Display;
        typedef unsigned long XID;
        typedef XID Window;
        typedef struct {
            int x, y;
            int width, height;
            int border_width;
            int depth;
        } XWindowAttributes;

        Display *XOpenDisplay(const char *display_name);
        int      XCloseDisplay(Display *display);
        int      XGetWindowAttributes(Display *display, Window w,
                                      XWindowAttributes *attr);
        Window   XDefaultRootWindow(Display *display);
        int      XDefaultScreen(Display *display);
        int      XDisplayWidth(Display *display, int screen_number);
        int      XDisplayHeight(Display *display, int screen_number);
    ]]
end

-- ============================================================================
-- sysconf constants (POSIX)
-- ============================================================================
local _SC_NPROCESSORS_CONF  = 83   -- _SC_NPROCESSORS_CONF
local _SC_NPROCESSORS_ONLN  = 84   -- _SC_NPROCESSORS_ONLN
local _SC_PAGESIZE          = 30   -- _SC_PAGESIZE

-- ============================================================================
-- Helpers
-- ============================================================================

-- Human-readable byte size
local function fmt_bytes(bytes)
    if not bytes then return "N/A" end
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

-- Read entire file content as string (via ffi.C fopen/fgets/fclose)
local function read_file(path)
    local fp = ffi.C.fopen(path, "r")
    if fp == nil then return nil end
    local buf = ffi.new("char[4096]")
    local result = {}
    while ffi.C.fgets(buf, 4096, fp) ~= nil do
        result[#result + 1] = ffi.string(buf):gsub("\n$", "")
    end
    ffi.C.fclose(fp)
    return table.concat(result, "\n")
end

-- Read a single-line value from a file
local function read_file_line(path)
    local content = read_file(path)
    if content then
        return content:match("^%s*(.-)%s*$")
    end
    return nil
end

-- Convert uptime seconds to readable format
local function format_uptime(seconds)
    local diff = math.floor(seconds)
    if not diff or diff < 0 then return "N/A" end
    local days    = math.floor(diff / 86400)
    local hours   = math.floor((diff % 86400) / 3600)
    local minutes = math.floor((diff % 3600) / 60)
    local secs    = diff % 60
    return string.format("%d days %02d:%02d:%02d", days, hours, minutes, secs)
end

-- ============================================================================
-- 1. CPU Info  (sysconf via ffi.C + /proc/cpuinfo for model name)
-- ============================================================================

local function get_cpu_info()
    -- CPU model name: only available from /proc/cpuinfo (no native FFI call for this)
    local cpuinfo = read_file("/proc/cpuinfo")
    local brand = "Unknown"
    local physical = 0
    local seen_physical = {}

    if cpuinfo then
        brand = cpuinfo:match("model name%s*:%s*(.-\n)") or "Unknown"
        brand = brand:gsub("\n$", ""):gsub("^%s+", ""):gsub("%s+$", "")

        -- Count physical cores: unique (physical_id, core_id) pairs
        local cur_phys_id = 0
        for line in cpuinfo:gmatch("[^\n]+") do
            local pid = line:match("physical id%s*:%s*(%d+)")
            local cid = line:match("core id%s*:%s*(%d+)")
            if pid then cur_phys_id = tonumber(pid) end
            if cid then
                local key = tostring(cur_phys_id) .. ":" .. cid
                if not seen_physical[key] then
                    seen_physical[key] = true
                    physical = physical + 1
                end
            end
        end
    end

    -- Logical / online cores via sysconf()
    local logical = tonumber(ffi.C.sysconf(_SC_NPROCESSORS_CONF)) or physical
    local online  = tonumber(ffi.C.sysconf(_SC_NPROCESSORS_ONLN)) or logical

    if physical == 0 then
        physical = logical
    end

    -- CPU frequency: read from sysfs (no pure FFI equivalent)
    local freq_khz = read_file_line("/sys/devices/system/cpu/cpu0/cpufreq/base_frequency")
        or read_file_line("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq")
    local freq_str = "N/A"
    if freq_khz then
        local val = tonumber(freq_khz)
        if val then
            if val > 10000 then
                freq_str = string.format("%.1f GHz", val / 1e6)
            else
                freq_str = string.format("%.0f MHz", val)
            end
        end
    end

    -- Architecture from uname()
    local un = ffi.new("struct utsname")
    ffi.C.uname(un)
    local arch = ffi.string(un.machine)

    -- Page size from sysconf()
    local pagesize = tonumber(ffi.C.sysconf(_SC_PAGESIZE)) or 4096

    return {
        brand          = brand,
        arch           = arch,
        logical_cores  = logical,
        physical_cores = physical,
        online_cores   = online,
        frequency      = freq_str,
        page_size      = string.format("%.0f KB", pagesize / 1024),
    }
end

-- ============================================================================
-- 2. Memory Info  (sysinfo via ffi.C / libc)
-- ============================================================================

local function get_memory_info()
    local si = ffi.new("struct sysinfo")
    if ffi.C.sysinfo(si) ~= 0 then
        return {
            total = "N/A", used = "N/A", available = "N/A",
            load_percent = 0, free = "N/A", buffers = "N/A",
            swap_total = "N/A", swap_used = "N/A", swap_free = "N/A",
        }
    end

    local mem_unit    = tonumber(si.mem_unit)
    local total_bytes = tonumber(si.totalram) * mem_unit
    local free_bytes  = tonumber(si.freeram) * mem_unit
    local buff_bytes  = tonumber(si.bufferram) * mem_unit
    local swap_total  = tonumber(si.totalswap) * mem_unit
    local swap_free   = tonumber(si.freeswap) * mem_unit

    local used_bytes  = total_bytes - free_bytes - buff_bytes
    local swap_used   = swap_total - swap_free
    local load_pct    = total_bytes > 0 and math.floor(used_bytes / total_bytes * 100 + 0.5) or 0

    return {
        total        = fmt_bytes(total_bytes),
        used         = fmt_bytes(used_bytes),
        available    = fmt_bytes(free_bytes + buff_bytes),
        load_percent = load_pct,
        free         = fmt_bytes(free_bytes),
        buffers      = fmt_bytes(buff_bytes),
        swap_total   = fmt_bytes(swap_total),
        swap_used    = fmt_bytes(swap_used),
        swap_free    = fmt_bytes(swap_free),
    }
end

-- ============================================================================
-- 3. OS Version  (uname via ffi.C + /etc/os-release for distro)
-- ============================================================================

local function get_os_version()
    local un = ffi.new("struct utsname")
    ffi.C.uname(un)

    local release = ffi.string(un.release)
    local sysname = ffi.string(un.sysname)
    local kver    = ffi.string(un.version)
    local machine = ffi.string(un.machine)

    -- Distro name from /etc/os-release (no pure FFI equivalent, file is standard)
    local os_release = read_file("/etc/os-release")
    local distro_v = ""
    if os_release then
        local name = os_release:match('PRETTY_NAME="([^"]*)"')
                    or os_release:match("PRETTY_NAME=([^\n]*)")
        if name then
            distro_v = name
        else
            local id  = os_release:match('ID="?([^"\n]+)"?')
            local ver = os_release:match('VERSION_ID="?([^"\n]+)"?')
            if id and ver then
                distro_v = id .. " " .. ver
            elseif id then
                distro_v = id
            end
        end
    end

    if distro_v == "" then
        local lsb = read_file("/etc/lsb-release")
        if lsb then
            local desc = lsb:match('DISTRIB_DESCRIPTION="([^"]*)"')
            if desc then distro_v = desc end
        end
    end

    return {
        system    = sysname,
        distro    = distro_v ~= "" and distro_v or "Linux",
        release   = release,
        version   = kver,
        machine   = machine,
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
-- 5. System Uptime  (sysinfo via ffi.C / libc)
-- ============================================================================

local function get_uptime()
    local si = ffi.new("struct sysinfo")
    if ffi.C.sysinfo(si) ~= 0 then
        return "N/A"
    end
    return format_uptime(tonumber(si.uptime))
end

-- ============================================================================
-- 6. Screen Resolution  (libX11.so.6 or fallback to /sys/class/drm)
-- ============================================================================

local function get_screen_info()
    -- Primary: try X11 library (ffi.load)
    if libX11 then
        local ok, display = pcall(function()
            return libX11.XOpenDisplay(nil)
        end)
        if ok and display ~= nil then
            local screen_num = libX11.XDefaultScreen(display)
            local w = tonumber(libX11.XDisplayWidth(display, screen_num))
            local h = tonumber(libX11.XDisplayHeight(display, screen_num))
            libX11.XCloseDisplay(display)
            if w and h and w > 0 and h > 0 then
                return {
                    width      = w,
                    height     = h,
                    resolution = string.format("%d x %d", w, h),
                }
            end
        end
    end

    -- Fallback: scan /sys/class/drm for connected displays
    local max_w, max_h = 0, 0
    local has_display = false

    for card_n = 0, 9 do
        for _, conn_type in ipairs({"HDMI-A", "DP", "eDP", "LVDS", "VGA", "DVI-I", "DVI-D", "Virtual"}) do
            for suffix = 1, 4 do
                local conn_path = string.format(
                    "/sys/class/drm/card%d/card%d-%s-%d/",
                    card_n, card_n, conn_type, suffix
                )
                local status = read_file_line(conn_path .. "status")
                if status == "connected" then
                    has_display = true
                    local modes = read_file(conn_path .. "modes")
                    if modes then
                        for line in modes:gmatch("[^\n]+") do
                            local w, h = line:match("^(%d+)x(%d+)")
                            if w and h then
                                w, h = tonumber(w), tonumber(h)
                                if w * h > max_w * max_h then
                                    max_w, max_h = w, h
                                end
                            end
                        end
                    end
                end
            end
        end
    end

    if has_display then
        return {
            width      = max_w,
            height     = max_h,
            resolution = string.format("%d x %d", max_w, max_h),
        }
    end

    return { resolution = "N/A (no display detected)" }
end

-- ============================================================================
-- 7. Battery / Power  (/sys/class/power_supply — no pure FFI equivalent)
-- ============================================================================

local function get_power_info()
    -- Try BAT0, then BAT1
    local bat_path
    for _, bat in ipairs({"BAT0", "BAT1"}) do
        local p = "/sys/class/power_supply/" .. bat
        local present = read_file_line(p .. "/present")
        local status  = read_file_line(p .. "/status")
        if status and present ~= "0" then
            bat_path = p
            break
        end
    end

    if not bat_path then
        -- Check AC power
        for _, ac in ipairs({"AC0", "ACAD", "AC"}) do
            local online = read_file_line("/sys/class/power_supply/" .. ac .. "/online")
            if online == "1" then
                return { source = "AC Power (no battery)", battery = "N/A", type = "N/A" }
            end
        end
        return { source = "N/A (desktop / unknown)", battery = "N/A", type = "N/A" }
    end

    local status       = read_file_line(bat_path .. "/status")
    local capacity     = read_file_line(bat_path .. "/capacity")
    local charge_full  = read_file_line(bat_path .. "/charge_full")
    local charge_now   = read_file_line(bat_path .. "/charge_now")
    local energy_full  = read_file_line(bat_path .. "/energy_full")
    local energy_now   = read_file_line(bat_path .. "/energy_now")
    local tech         = read_file_line(bat_path .. "/technology")

    local source_map = {
        Charging     = "AC Power (Charging)",
        Full         = "AC Power (Charged)",
        Discharging  = "Battery Power",
        ["Not charging"] = "AC Power (Not charging)",
    }
    local source = source_map[status] or "Unknown"

    -- Battery percentage
    local batt_pct = nil
    if capacity and tonumber(capacity) then
        batt_pct = tonumber(capacity)
    elseif charge_full and charge_now and tonumber(charge_full) and tonumber(charge_full) > 0 then
        batt_pct = math.floor(tonumber(charge_now) / tonumber(charge_full) * 100 + 0.5)
    elseif energy_full and energy_now and tonumber(energy_full) and tonumber(energy_full) > 0 then
        batt_pct = math.floor(tonumber(energy_now) / tonumber(energy_full) * 100 + 0.5)
    end

    return {
        source  = source,
        battery = batt_pct and string.format("%d%%", batt_pct) or "N/A",
        type    = tech or "Unknown",
    }
end

-- ============================================================================
-- 8. Disk Info  (statfs via ffi.C / libc)
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
        total       = fmt_bytes(total_bytes),
        used        = fmt_bytes(used_bytes),
        available   = fmt_bytes(free_bytes),
        use_pct     = use_pct .. "%",
        total_files = files,
        free_files  = ffree,
    }
end

-- ============================================================================
-- 9. Environment Variables  (getenv via ffi.C / libc)
-- ============================================================================

local function get_env()
    local function e(key)
        local p = ffi.C.getenv(key)
        return (p ~= nil) and ffi.string(p) or "N/A"
    end

    return {
        HOME              = e("HOME"),
        USER              = e("USER"),
        SHELL             = e("SHELL"),
        TMPDIR            = e("TMPDIR"),
        LANG              = e("LANG"),
        DISPLAY           = e("DISPLAY"),
        XDG_SESSION_TYPE  = e("XDG_SESSION_TYPE"),
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
    print_separator("LuaJIT FFI — Linux System Information")
    print()
    print("  Libraries loaded: ffi.C (libc)" .. (libX11 and "  libX11.so.6" or ""))
    print()

    -- 1. CPU
    local cpu = get_cpu_info()
    print_separator("CPU Info")
    print_kv("Processor",        cpu.brand)
    print_kv("Architecture",     cpu.arch)
    print_kv("Logical Cores",    cpu.logical_cores)
    print_kv("Physical Cores",   cpu.physical_cores)
    print_kv("Online Cores",     cpu.online_cores)
    print_kv("Frequency",        cpu.frequency)
    print_kv("Page Size",        cpu.page_size)
    print()

    -- 2. Memory
    local mem = get_memory_info()
    print_separator("Memory Info  [sysinfo()]")
    print_kv("Total Physical",   mem.total)
    print_kv("Used",             mem.used)
    print_kv("Available",        mem.available)
    print_kv("Memory Load",      tostring(mem.load_percent) .. "%")
    print_kv("Free",             mem.free)
    print_kv("Buffers/Cached",   mem.buffers)
    print_kv("Swap Total",       mem.swap_total)
    print_kv("Swap Used",        mem.swap_used)
    print_kv("Swap Free",        mem.swap_free)
    print()

    -- 3. OS
    local osv = get_os_version()
    print_separator("OS Version")
    print_kv("System",           osv.system)
    print_kv("Distribution",     osv.distro)
    print_kv("Kernel Release",   osv.release)
    print_kv("Kernel Version",   osv.version)
    print_kv("Machine",          osv.machine)
    print()

    -- 4. Host / User
    local names = get_system_names()
    print_separator("System Identity")
    print_kv("Hostname",         names.hostname)
    print_kv("Current User",     names.username)
    print()

    -- 5. Display
    local label = libX11 and "Display  [libX11.so.6]" or "Display  [/sys/class/drm]"
    local screen = get_screen_info()
    print_separator(label)
    print_kv("Resolution",       screen.resolution)
    print()

    -- 6. Uptime
    print_separator("Uptime  [sysinfo()]")
    print_kv("Up Time",          get_uptime())
    print()

    -- 7. Battery
    local power = get_power_info()
    print_separator("Power Status  [/sys/class/power_supply]")
    print_kv("Type",             power.type or "N/A")
    print_kv("Power Source",     power.source)
    print_kv("Battery Level",    power.battery)
    print()

    -- 8. Disk
    local disk = get_disk_info()
    if disk.total then
        print_separator("Root Disk  [statfs /]")
        print_kv("Total",            disk.total)
        print_kv("Used",             disk.used)
        print_kv("Available",        disk.available)
        print_kv("Usage",            disk.use_pct)
        print_kv("Total Files",      disk.total_files)
        print_kv("Free Files",       disk.free_files)
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

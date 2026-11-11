import os

cat_cpp_path = r"C:\Users\ayubg\AndroidStudioProjects\NewBlackbox-main1\NewBlackbox-main\Bcore\src\main\cpp\catEngine\catEngine.cpp"
with open(cat_cpp_path, "r", encoding="utf-8") as f:
    code = f.read()

# 1. Update isCarromApp()
target_is_carrom = """bool isCarromApp() {
    std::ifstream cmdlineFile("/proc/self/cmdline");
    if (!cmdlineFile.is_open()) return false;
    std::string pkg;
    std::getline(cmdlineFile, pkg);
    if (!pkg.empty()) {
        size_t pos = pkg.find('\\0');
        if (pos != std::string::npos) {
            pkg = pkg.substr(0, pos);
        }
    }
    return pkg.find("com.miniclip.carrom") != std::string::npos;
}"""

repl_is_carrom = """// Forward declaration
uintptr_t resolveLibgameBase();

bool isCarromApp() {
    // 1. If libgame.so is loaded in memory in this process, it IS Carrom Pool!
    if (resolveLibgameBase() != 0) return true;

    // 2. Check cmdline for carrom or Blackbox virtual container process (:p0, :p1, etc.)
    std::ifstream cmdlineFile("/proc/self/cmdline");
    if (cmdlineFile.is_open()) {
        std::string pkg;
        std::getline(cmdlineFile, pkg);
        if (pkg.find("com.miniclip.carrom") != std::string::npos ||
            pkg.find(":p") != std::string::npos ||
            pkg.find("blackbox") != std::string::npos) {
            return true;
        }
    }
    return false;
}"""

if target_is_carrom in code:
    code = code.replace(target_is_carrom, repl_is_carrom)
elif target_is_carrom.replace("\n", "\r\n") in code:
    code = code.replace(target_is_carrom.replace("\n", "\r\n"), repl_is_carrom.replace("\n", "\r\n"))
    print("Replaced isCarromApp (CRLF)")
else:
    print("target_is_carrom not found!")

# 2. Update resolveLibgameBase()
target_resolve = """// Robust libgame base address resolver
uintptr_t resolveLibgameBase() {
    uintptr_t phdrBase = 0;
    dl_iterate_phdr([](struct dl_phdr_info* info, size_t size, void* data) -> int {
        if (!info || !info->dlpi_name) return 0;
        if (info->dlpi_name &&
            (strstr(info->dlpi_name, "CARROM") || strstr(info->dlpi_name, "split_config")) &&
            strstr(info->dlpi_name, "libgame") &&
            !strstr(info->dlpi_name, "libgamemanager")) {
            uintptr_t b = static_cast<uintptr_t>(info->dlpi_addr);
            if (b != 0) {
                uint32_t magic = 0;
                if (safeReadInsn(b, &magic) && magic == 0x464C457F) {
                    *reinterpret_cast<uintptr_t*>(data) = b;
                    return 1;
                }
            }
        }
        return 0;
    }, &phdrBase);

    if (phdrBase != 0) return phdrBase;

    std::ifstream maps("/proc/self/maps");
    if (!maps.is_open()) return 0;
    std::string line;
    while (std::getline(maps, line)) {
        if ((line.find("CARROM") != std::string::npos || line.find("split_config") != std::string::npos) &&
            line.find("libgame") != std::string::npos &&
            line.find("libgamemanager") == std::string::npos &&
            line.find("r-xp") != std::string::npos) {"""

repl_resolve = """// Robust libgame base address resolver
uintptr_t resolveLibgameBase() {
    uintptr_t phdrBase = 0;
    dl_iterate_phdr([](struct dl_phdr_info* info, size_t size, void* data) -> int {
        if (!info || !info->dlpi_name) return 0;
        const char* name = info->dlpi_name;
        if (strstr(name, "libgame") && !strstr(name, "libgamemanager")) {
            uintptr_t b = static_cast<uintptr_t>(info->dlpi_addr);
            if (b != 0) {
                uint32_t magic = 0;
                if (safeReadInsn(b, &magic) && magic == 0x464C457F) {
                    *reinterpret_cast<uintptr_t*>(data) = b;
                    return 1;
                }
            }
        }
        return 0;
    }, &phdrBase);

    if (phdrBase != 0) return phdrBase;

    std::ifstream maps("/proc/self/maps");
    if (!maps.is_open()) return 0;
    std::string line;
    while (std::getline(maps, line)) {
        if (line.find("libgame") != std::string::npos &&
            line.find("libgamemanager") == std::string::npos &&
            line.find("r-xp") != std::string::npos) {"""

if target_resolve in code:
    code = code.replace(target_resolve, repl_resolve)
elif target_resolve.replace("\n", "\r\n") in code:
    code = code.replace(target_resolve.replace("\n", "\r\n"), repl_resolve.replace("\n", "\r\n"))
    print("Replaced resolveLibgameBase (CRLF)")
else:
    print("target_resolve not found!")

# 3. Update file logging destinations in logShotTelemetry
target_log_file = """    // Persist full report to disk
    FILE* logF = fopen("/sdcard/Download/carrom_shot_telemetry.log", "a");
    if (!logF) {
        logF = fopen("/data/data/com.miniclip.carrom/files/shot_telemetry.log", "a");
    }
    if (logF) {
        fwrite(report.c_str(), 1, report.length(), logF);
        fclose(logF);
    }"""

repl_log_file = """    // Persist full report to disk across multiple robust paths
    const char* logPaths[] = {
        "/sdcard/Download/carrom_shot_telemetry.log",
        "/data/data/top.niunaijun.blackboxa/files/carrom_shot_telemetry.log",
        "/data/user/0/top.niunaijun.blackboxa/files/carrom_shot_telemetry.log",
        "/data/data/com.miniclip.carrom/files/shot_telemetry.log"
    };
    for (const char* lp : logPaths) {
        FILE* logF = fopen(lp, "a");
        if (logF) {
            fwrite(report.c_str(), 1, report.length(), logF);
            fclose(logF);
        }
    }"""

if target_log_file in code:
    code = code.replace(target_log_file, repl_log_file)
elif target_log_file.replace("\n", "\r\n") in code:
    code = code.replace(target_log_file.replace("\n", "\r\n"), repl_log_file.replace("\n", "\r\n"))
    print("Replaced log_file (CRLF)")
else:
    print("target_log_file not found!")

with open(cat_cpp_path, "w", encoding="utf-8") as f:
    f.write(code)
print("Updated catEngine.cpp successfully!")

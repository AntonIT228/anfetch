#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <filesystem>
#include <sys/statvfs.h>
#include <cstring>
#include <iomanip>
#include <sys/utsname.h>
#include <cstdlib>
#include <chrono>
#include <ctime>
#include <sys/stat.h>
#include <algorithm>
#include <functional>
#include <set>
#include <regex>


namespace Color {
    constexpr const char* RESET = "\033[0m";
    constexpr const char* BOLD = "\033[1m";
    constexpr const char* RED = "\033[31m";
    constexpr const char* GREEN = "\033[32m";
    constexpr const char* YELLOW = "\033[33m";
    constexpr const char* BLUE = "\033[34m";
    constexpr const char* MAGENTA = "\033[35m";
    constexpr const char* CYAN = "\033[36m";
    constexpr const char* WHITE = "\033[37m";
    constexpr const char* BRIGHT_CYAN = "\033[96m";
    constexpr const char* BRIGHT_GREEN = "\033[92m";
    constexpr const char* BRIGHT_YELLOW = "\033[93m";
    constexpr const char* BRIGHT_MAGENTA = "\033[95m";
}


size_t g_label_width = 9;


std::string get_os_name() {
    std::ifstream file("/etc/os-release");
    if (!file.is_open()) return "Unknown Linux OS";
    std::string line;
    while (std::getline(file, line)) {
        if (line.rfind("PRETTY_NAME=", 0) == 0) {
            size_t first = line.find('"');
            size_t last = line.rfind('"');
            if (first != std::string::npos && last != std::string::npos && first != last) {
                file.close();
                return line.substr(first + 1, last - first - 1);
            }
        }
    }
    file.close();
    return "Linux OS";
}

std::string get_host_model() {
    std::ifstream vendor_file("/sys/class/dmi/id/sys_vendor");
    std::ifstream product_file("/sys/class/dmi/id/product_name");
    
    std::string vendor, product;
    if (vendor_file) std::getline(vendor_file, vendor);
    if (product_file) std::getline(product_file, product);
    
    if (vendor.empty() && product.empty()) return "Unknown host";
    return vendor + " " + product;
}

std::string get_os_id() {
    std::ifstream file("/etc/os-release");
    if (!file.is_open()) return "linux";
    std::string line;
    while (std::getline(file, line)) {
        if (line.rfind("ID=", 0) == 0) {
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                std::string id = line.substr(pos + 1);
                if (!id.empty() && id.front() == '"') id.erase(0, 1);
                if (!id.empty() && id.back() == '"')  id.pop_back();
                file.close();
                return id;
            }
        }
    }
    file.close();
    return "linux";
}

std::string get_cpu_info() {
    std::ifstream file("/proc/cpuinfo");
    if (!file.is_open()) return "Unknown CPU";
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.rfind("model name", 0) == 0) {
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                return line.substr(pos + 2);
            }
        }
    }
    return "Unknown CPU";
}

std::string clean_gpu_name(std::string name) {
    size_t rev_pos = name.rfind(" (rev ");
    if (rev_pos != std::string::npos) {
        name.erase(rev_pos);
    }

    size_t corp_pos = name.find(" Corporation ");
    if (corp_pos != std::string::npos) {
        name.replace(corp_pos, 13, " ");
    }

    size_t lb = name.find('[');
    size_t rb = name.find(']');
    if (lb != std::string::npos && rb != std::string::npos && rb > lb) {
        std::string inside = name.substr(lb + 1, rb - lb - 1);

        std::string vendor;
        if (name.rfind("NVIDIA", 0) == 0)       vendor = "NVIDIA";
        else if (name.rfind("Intel", 0) == 0)   vendor = "Intel";
        else if (name.rfind("AMD", 0) == 0 ||
                 name.rfind("Advanced Micro hosts", 0) == 0) vendor = "AMD";

        if (!vendor.empty() && inside.rfind(vendor, 0) != 0) {
            return vendor + " " + inside;
        }
        return inside;
    }

    std::string result;
    bool last_space = false;
    for (char c : name) {
        if (c == ' ') {
            if (!last_space) result += c;
            last_space = true;
        } else {
            result += c;
            last_space = false;
        }
    }

    while (!result.empty() && result.back() == ' ') result.pop_back();

    return result;
}

std::vector<std::string> get_gpu_list() {
    std::vector<std::string> gpus;
    std::string drm_dir = "/sys/class/drm";

    std::error_code ec;
    if (!std::filesystem::exists(drm_dir, ec) || ec) return gpus;

    for (const auto& entry : std::filesystem::directory_iterator(drm_dir, ec)) {
        if (ec) break;
        std::string name = entry.path().filename().string();
        if (name.rfind("card", 0) == 0 && name.find('-') == std::string::npos) {
            std::string host_name_path = entry.path().string() + "/host/host_name";
            std::ifstream name_file(host_name_path);

            if (name_file.is_open()) {
                std::string gpu_name;
                std::getline(name_file, gpu_name);
                name_file.close();

                if (!gpu_name.empty()) {
                    gpus.push_back(clean_gpu_name(gpu_name));
                }
            }
        }
    }

    if (gpus.empty()) {
        FILE* pipe = popen("lspci -d ::0300 2>/dev/null", "r");
        if (pipe) {
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                std::string line(buffer);
                if (!line.empty() && line.back() == '\n') line.pop_back();
                size_t pos = line.rfind(':');
                if (pos != std::string::npos) {
                    gpus.push_back(clean_gpu_name(line.substr(pos + 2)));
                }
            }
            pclose(pipe);
        }
    }

    return gpus;
}

std::string get_ram_info() {
    std::ifstream file("/proc/meminfo");
    if (!file.is_open()) return "Unknown memory";

    std::string line;
    long long total_kb = 0;
    long long avail_kb = 0;

    while (std::getline(file, line)) {
        if (line.rfind("MemTotal:", 0) == 0) {
            std::stringstream ss(line);
            std::string trash;
            ss >> trash >> total_kb; 
        }
        else if (line.rfind("MemAvailable:", 0) == 0) {
            std::stringstream ss(line);
            std::string trash;
            ss >> trash >> avail_kb;
        }

        if (total_kb > 0 && avail_kb > 0) {
            break;
        }
    }
    file.close();

    if (total_kb == 0 || avail_kb == 0) return "Unknown memory";

    long long used_kb = total_kb - avail_kb;

    double total_gib = (double)total_kb / 1048576.0;
    double used_gib  = (double)used_kb  / 1048576.0;
    int percentage_ram = (total_gib > 0) ? (int)((used_gib / total_gib) * 100.0) : 0;

    char result[64];
    std::snprintf(result, sizeof(result), "%.1f GiB / %.1f GiB (%d%%)", used_gib, total_gib, percentage_ram);

    return std::string(result);
}

std::vector<std::string> get_disks_info() {
    std::vector<std::string> disks;
    std::error_code ec;

    if (!std::filesystem::exists("/sys/block/", ec) || ec) return disks;

    for (const auto& entry : std::filesystem::directory_iterator("/sys/block/", ec)) {
        if (ec) break;
        std::string disk_name = entry.path().filename().string();

        if (disk_name.rfind("loop", 0) == 0 || disk_name.rfind("ram", 0) == 0) {
            continue;
        }

        std::string size_path = "/sys/block/" + disk_name + "/size";
        std::string type_path = "/sys/block/" + disk_name + "/queue/rotational";
        
        std::ifstream size_file(size_path);
        long long sectors = 0;
        size_file >> sectors;
        size_file.close();

        double size_gb = (double)sectors / 2097152.0;

        std::ifstream type_file(type_path);
        int is_rotational = 0;
        type_file >> is_rotational;
        type_file.close();

        std::string disk_type = (is_rotational == 1) ? "HDD" : "SSD";

        char buffer[128];
        std::snprintf(buffer, sizeof(buffer), "%s (%.1f GB %s)", disk_name.c_str(), size_gb, disk_type.c_str());

        disks.push_back(std::string(buffer));
    }

    return disks;
}

bool is_real_filesystem(const std::string& fstype) {
    static const std::set<std::string> pseudo_fs = {
        "proc", "sysfs", "devtmpfs", "devpts", "tmpfs", "ramfs",
        "cgroup", "cgroup2", "pstore", "bpf", "debugfs", "tracefs",
        "securityfs", "configfs", "fusectl", "mqueue", "hugetlbfs",
        "autofs", "binfmt_misc", "rpc_pipefs", "nsfs", "overlay",
        "squashfs", "fuse.gvfsd-fuse", "fuse.portal", "efivarfs",
        "selinuxfs", "systemd-1", "sunrpc", "nfsd"
    };
    return pseudo_fs.find(fstype) == pseudo_fs.end();
}

std::string unescape_mount_path(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 3 < s.size()) {
            std::string code = s.substr(i + 1, 3);
            if (code == "040") { out += ' '; i += 3; continue; }
            if (code == "011") { out += '\t'; i += 3; continue; }
            if (code == "012") { out += '\n'; i += 3; continue; }
            if (code == "134") { out += '\\'; i += 3; continue; }
        }
        out += s[i];
    }
    return out;
}

std::vector<std::string> get_storage_info() {
    std::vector<std::string> result;

    std::ifstream mounts("/proc/mounts");
    if (!mounts.is_open()) return { "Unknown Storage" };

    std::set<std::string> seen_hosts;
    std::set<std::string> seen_mounts;

    std::string line;
    while (std::getline(mounts, line)) {
        std::istringstream ss(line);
        std::string host, mountpoint, fstype, options;
        if (!(ss >> host >> mountpoint >> fstype >> options)) continue;

        mountpoint = unescape_mount_path(mountpoint);

        if (!is_real_filesystem(fstype)) continue;

        if (host.rfind("/dev/loop", 0) == 0) continue;
        if (mountpoint.rfind("/snap/", 0) == 0) continue;
        if (mountpoint == "/boot" || mountpoint == "/boot/efi" ||
            mountpoint == "/efi" || mountpoint == "/var/lib/docker") continue;

        if (seen_hosts.count(host)) continue;
        if (seen_mounts.count(mountpoint)) continue;
        seen_hosts.insert(host);
        seen_mounts.insert(mountpoint);

        struct statvfs stat;
        if (statvfs(mountpoint.c_str(), &stat) != 0) continue;

        double block_size = stat.f_frsize;
        if (block_size == 0 || stat.f_blocks == 0) continue;

        double total_gib = (stat.f_blocks * block_size) / 1073741824.0;
        double free_gib  = (stat.f_bfree * block_size) / 1073741824.0;
        double used_gib  = total_gib - free_gib;
        int percentage_storage = (total_gib > 0) ? (int)((used_gib / total_gib) * 100.0) : 0;

        char buffer[512];
        if (mountpoint == "/") {
            std::snprintf(buffer, sizeof(buffer),
                          "%.2f GiB / %.2f GiB (%d%%) (%s)",
                          used_gib, total_gib, percentage_storage, fstype.c_str());
        } else {
            std::snprintf(buffer, sizeof(buffer),
                          "%.2f GiB / %.2f GiB (%d%%) (%s) @ (%s)",
                          used_gib, total_gib, percentage_storage, fstype.c_str(),
                          mountpoint.c_str());
        }
        result.push_back(std::string(buffer));
    }
    mounts.close();

    if (result.empty()) result.push_back("Unknown Storage");
    return result;
}

std::string get_uptime_info() {
    std::ifstream file("/proc/uptime");
    if (!file.is_open()) return "Unknown uptime";

    double total_seconds = 0;
    file >> total_seconds;
    file.close();
    
    int total_minutes = (int)total_seconds / 60;
    int minutes = total_minutes % 60;
    int hours = total_minutes / 60;

    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%d hour%s %d min",
                  hours, hours == 1 ? "" : "s", minutes);

    return std::string(buffer);
}

std::string get_kernel_info() {
    struct utsname buffer;
    if (uname(&buffer) != 0) return "Unknown Kernel";
    return std::string(buffer.release);
}

std::string get_packages_info() {
    int official_count = 0;
    int aur_count = 0;
    std::string pacman_dir = "/var/lib/pacman/local";

    std::error_code ec;
    if (!std::filesystem::exists(pacman_dir, ec) || ec) {
        return "Unknown";
    }

    for (const auto& entry : std::filesystem::directory_iterator(pacman_dir, ec)) {
        if (ec) break;
        if (!entry.is_directory()) continue;

        std::ifstream desc_file(entry.path().string() + "/desc");
        if (!desc_file.is_open()) {
            official_count++;
            continue;
        }

        std::string line;
        bool is_aur = true;
        bool repo_found = false;

        while (std::getline(desc_file, line)) {
            if (line == "%REPOSITORY%") {
                if (std::getline(desc_file, line)) {
                    repo_found = true;
                    if (line.rfind("core", 0) == 0 ||
                        line.rfind("extra", 0) == 0 ||
                        line.rfind("multilib", 0) == 0 ||
                        line == "community") {
                        is_aur = false;
                    }
                }
                break;
            }
        }

        if (!repo_found) {
            official_count++;
        } else if (is_aur) {
            aur_count++;
        } else {
            official_count++;
        }
    }

    std::string result = std::to_string(official_count) + " (pacman)";
    if (aur_count > 0) {
        result += ", " + std::to_string(aur_count) + " (aur)";
    }
    return result;
}

std::string get_wm_de_session_info() {
    const char* de = std::getenv("XDG_CURRENT_DESKTOP");
    const char* session = std::getenv("XDG_SESSION_TYPE");
    
    std::string result = (de ? de : "Unknown WM/DE");
    if (session) {
        result += " (" + std::string(session) + ")";
    }
    return result;
}

std::string get_shell_info() {
    const char* shell_path = std::getenv("SHELL");
    if (!shell_path) return "Unknown Shell";

    std::string shell_name = std::filesystem::path(shell_path).filename().string();
    std::string version;

    std::string cmd = std::string(shell_path) + " --version 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (pipe) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string line(buffer);
            std::regex version_regex(R"((\d+\.\d+(?:\.\d+)?))");
            std::smatch match;
            if (std::regex_search(line, match, version_regex)) {
                version = match[1];
            }
        }
        pclose(pipe);
    }

    if (!version.empty()) {
        return shell_name + " " + version;
    }
    return shell_name;
}

std::string get_terminal_info() {
    const char* term = std::getenv("TERM_PROGRAM");
    if (!term) term = std::getenv("TERMINAL");
    if (!term) term = std::getenv("TERM");
    return term ? term : "Unknown Terminal";
}

std::string get_current_datetime() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    
    char buffer[80];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&now_time));
    
    return std::string(buffer);
}

std::string get_os_age() {
    std::ifstream log("/var/log/pacman.log");
    if (!log.is_open()) return "Unknown Age";

    std::string first_line;
    std::getline(log, first_line);
    log.close();

    if (first_line.empty()) return "Unknown Age";

    size_t start = first_line.find('[');
    size_t end = first_line.find('T'); 
    if (start == std::string::npos || end == std::string::npos) return "Unknown Age";

    std::string date_str = first_line.substr(start + 1, end - start - 1); 

    struct tm tm = {};
    if (strptime(date_str.c_str(), "%Y-%m-%d", &tm) == nullptr) return "Unknown Age";

    std::time_t install_time = mktime(&tm);
    std::time_t now = std::time(nullptr);
    int days = (now - install_time) / (60 * 60 * 24);

    return std::to_string(days) + " days";
}


size_t get_utf8_visible_length(const std::string& str) {
    size_t length = 0;
    for (size_t i = 0; i < str.length(); ) {
        unsigned char c = str[i];
        if (c >= 0 && c <= 127) i += 1;
        else if ((c & 0xE0) == 0xC0) i += 2;
        else if ((c & 0xF0) == 0xE0) i += 3;
        else if ((c & 0xF8) == 0xF0) i += 4;
        else i += 1;
        length++;
    }
    return length;
}

std::string make_info_line(const std::string& label, const std::string& value, const char* label_color) {
    std::string padded = label + ":";
    if (padded.size() < g_label_width) {
        padded.append(g_label_width - padded.size(), ' ');
    } else {
        padded += ' ';
    }
    return std::string(label_color) + padded + Color::RESET + value;
}


struct Module {
    std::string  name;
    std::function<std::vector<std::string>()> render;
    bool         enabled_default;
};

std::vector<Module> build_modules() {
    return {

        {"host_model", []{
            return std::vector<std::string>{
                make_info_line("PC", get_host_model(), Color::BRIGHT_GREEN)
            };
        }, true},
        
        {"cpu", []{
            return std::vector<std::string>{
                make_info_line("CPU", get_cpu_info(), Color::BRIGHT_GREEN)
            };
        }, true},

        {"gpu", []{
            std::vector<std::string> out;
            auto gpus = get_gpu_list();
            for (size_t i = 0; i < gpus.size(); ++i) {
                std::string label = (gpus.size() > 1)
                    ? "GPU [" + std::to_string(i + 1) + "]"
                    : "GPU";
                out.push_back(make_info_line(label, gpus[i], Color::BRIGHT_GREEN));
            }
            return out;
        }, true},

        {"ram", []{
            return std::vector<std::string>{
                make_info_line("RAM", get_ram_info(), Color::BRIGHT_GREEN)
            };
        }, true},

        {"storage", []{
            std::vector<std::string> out;
            auto storages = get_storage_info();
            for (size_t i = 0; i < storages.size(); ++i) {
                std::string label = (storages.size() > 1)
                    ? "Storage [" + std::to_string(i + 1) + "]"
                    : "Storage";
                out.push_back(make_info_line(label, storages[i], Color::BRIGHT_GREEN));
            }
            return out;
        }, true},

        {"disks", []{
            std::vector<std::string> out;
            auto disks = get_disks_info();
            for (size_t i = 0; i < disks.size(); ++i) {
                out.push_back(make_info_line("Disk [" + std::to_string(i + 1) + "]",
                                             disks[i], Color::BRIGHT_GREEN));
            }
            return out;
        }, false},

        {"os", []{
            return std::vector<std::string>{
                make_info_line("OS", get_os_name(), Color::BRIGHT_YELLOW)
            };
        }, true},

        {"kernel", []{
            return std::vector<std::string>{
                make_info_line("Kernel", get_kernel_info(), Color::BRIGHT_YELLOW)
            };
        }, true},

        {"packages", []{
            return std::vector<std::string>{
                make_info_line("Packages", get_packages_info(), Color::BRIGHT_YELLOW)
            };
        }, true},

        {"shell", []{
            return std::vector<std::string>{
                make_info_line("Shell", get_shell_info(), Color::BRIGHT_YELLOW)
            };
        }, true},

        {"session", []{
            return std::vector<std::string>{
                make_info_line("Session", get_wm_de_session_info(), Color::BRIGHT_YELLOW)
            };
        }, true},

        {"terminal", []{
            return std::vector<std::string>{
                make_info_line("Term", get_terminal_info(), Color::BRIGHT_YELLOW)
            };
        }, true},

        {"os_age", []{
            return std::vector<std::string>{
                make_info_line("OS Age", get_os_age(), Color::BRIGHT_CYAN)
            };
        }, true},
        
        {"uptime", []{
            return std::vector<std::string>{
                make_info_line("Uptime", get_uptime_info(), Color::BRIGHT_CYAN)
            };
        }, true},

        {"time", []{
            return std::vector<std::string>{
                make_info_line("DateTime", get_current_datetime(), Color::BRIGHT_CYAN)
            };
        }, true},
    };
}



int main(int argc, char* argv[]) {
    std::vector<Module> modules = build_modules();

    std::vector<bool> enabled;
    for (auto& m : modules) enabled.push_back(m.enabled_default);

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: fetch [options]\n"
                      << "  --show-<module>  enable module\n"
                      << "  --no-<module>    disable module\n"
                      << "  --disks          enable disks (legacy)\n"
                      << "Modules: ";
            for (auto& m : modules) std::cout << m.name << " ";
            std::cout << "\n";
            return 0;
        }

        if (arg == "--disks") {
            for (size_t j = 0; j < modules.size(); ++j) {
                if (modules[j].name == "disks") { enabled[j] = true; break; }
            }
            continue;
        }

        bool is_show = arg.rfind("--show-", 0) == 0;
        bool is_no   = arg.rfind("--no-",   0) == 0;
        if (!is_show && !is_no) continue;

        std::string target = is_show ? arg.substr(7) : arg.substr(5);

        for (size_t j = 0; j < modules.size(); ++j) {
            if (modules[j].name == target) {
                enabled[j] = is_show;
                break;
            }
        }
    }

    g_label_width = 9;
    for (auto& m : modules) {
        if (m.name == "gpu" || m.name == "storage" || m.name == "disks") {
            g_label_width = std::max(g_label_width, (size_t)13);
        }
    }

    std::vector<std::string> logo_lines;
    std::string os_id = get_os_id();

    std::vector<std::string> logo_paths = {
        "logos/" + os_id + ".txt",
        "/usr/share/anfetch/logos/" + os_id + ".txt"
    };

    if (os_id.rfind("ubuntu", 0) == 0 || os_id == "ubuntu") {
        logo_paths.push_back("logos/ubuntu.txt");
        logo_paths.push_back("/usr/share/anfetch/logos/ubuntu.txt");
    }
    if (os_id == "pop") {
        logo_paths.push_back("logos/popos.txt");
        logo_paths.push_back("/usr/share/anfetch/logos/popos.txt");
    }
    if (os_id == "mx") {
        logo_paths.push_back("logos/mxlinux.txt");
        logo_paths.push_back("/usr/share/anfetch/logos/mxlinux.txt");
    }
    if (os_id == "endeavouros") {
        logo_paths.push_back("logos/endeavour.txt");
        logo_paths.push_back("/usr/share/anfetch/logos/endeavour.txt");
    }
    if (os_id == "opensuse-tumbleweed") {
        logo_paths.push_back("logos/tumbleweed.txt");
        logo_paths.push_back("/usr/share/anfetch/logos/tumbleweed.txt");
    }
    if (os_id == "opensuse-leap") {
        logo_paths.push_back("logos/opensuse.txt");
        logo_paths.push_back("/usr/share/anfetch/logos/opensuse.txt");
    }
    if (os_id == "ghostbsd") {
        logo_paths.push_back("logos/ghost.txt");
        logo_paths.push_back("/usr/share/anfetch/logos/ghost.txt");
    }

    std::ifstream logo_file;
    for (const auto& path : logo_paths) {
        logo_file.open(path);
        if (logo_file.is_open()) break;
    }

    std::cout << "\n";
    
    if (logo_file.is_open()) {
        std::string line;
        while (std::getline(logo_file, line)) {
            logo_lines.push_back(line);
        }
        logo_file.close();
    } else {
        logo_lines.push_back("[ No LOGO found ]");
    }

    std::vector<std::string> info_lines;
    for (size_t i = 0; i < modules.size(); ++i) {
        if (!enabled[i]) continue;
        auto lines = modules[i].render();
        info_lines.insert(info_lines.end(), lines.begin(), lines.end());
    }

    size_t max_visible_width = 0;
    for (const auto& l : logo_lines) {
        size_t w = get_utf8_visible_length(l);
        if (w > max_visible_width) max_visible_width = w;
    }

    if (max_visible_width == 0) max_visible_width = 20;

    size_t max_rows = std::max(logo_lines.size(), info_lines.size());

    for (size_t i = 0; i < max_rows; ++i) {
        size_t current_visible_width = 0;

        if (i < logo_lines.size()) {
            std::cout << logo_lines[i];
            current_visible_width = get_utf8_visible_length(logo_lines[i]);
        }

        if (current_visible_width < max_visible_width) {
            std::cout << std::string(max_visible_width - current_visible_width, ' ');
        }
        
        std::cout << "   ";
        
        if (i < info_lines.size()) {
            std::cout << info_lines[i];
        }
        
        std::cout << std::endl;
    }

    return 0;
}
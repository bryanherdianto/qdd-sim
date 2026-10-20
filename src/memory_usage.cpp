#include "qdd/memory_usage.hpp"

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <sys/resource.h>
#include <unistd.h>
#include <fstream>
#include <string>
#endif

namespace qdd {

std::size_t get_peak_rss_bytes() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.PeakWorkingSetSize;
    }
    return 0;
#elif defined(__linux__) || defined(__APPLE__)
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
#if defined(__APPLE__)
        return static_cast<std::size_t>(usage.ru_maxrss);
#else
        return static_cast<std::size_t>(usage.ru_maxrss) * 1024;
#endif
    }
    return 0;
#else
    return 0;
#endif
}

std::size_t get_current_rss_bytes() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
    return 0;
#elif defined(__linux__)
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.rfind("VmRSS:", 0) == 0) {
            std::size_t kb = 0;
            if (sscanf(line.c_str(), "VmRSS: %zu kB", &kb) == 1) {
                return kb * 1024;
            }
        }
    }
    return 0;
#else
    return 0;
#endif
}

} // namespace qdd

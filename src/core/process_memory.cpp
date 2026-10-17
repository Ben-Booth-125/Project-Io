#include "process_memory.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <psapi.h> // GetProcessMemoryInfo (K32GetProcessMemoryInfo, kernel32, PSAPI_VERSION 2)
#endif

namespace io_mem {

process_memory query()
{
    process_memory m;
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX c{};
    c.cb = sizeof c;
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&c), sizeof c))
    {
        m.working_set      = static_cast<double>(c.WorkingSetSize);
        m.peak_working_set = static_cast<double>(c.PeakWorkingSetSize);
        m.private_bytes    = static_cast<double>(c.PrivateUsage);
        m.peak_private     = static_cast<double>(c.PeakPagefileUsage); // peak commit charge = peak private bytes
    }
#endif
    return m;
}

double thread_cpu_ms()
{
#ifdef _WIN32
    FILETIME c, e, k, u;
    if (!GetThreadTimes(GetCurrentThread(), &c, &e, &k, &u))
        return -1.0;
    const auto v = [](const FILETIME& f) {
        return static_cast<double>((static_cast<unsigned long long>(f.dwHighDateTime) << 32) | f.dwLowDateTime);
    };
    return (v(k) + v(u)) / 10000.0; // 100 ns units
#else
    return -1.0;
#endif
}

} // namespace io_mem

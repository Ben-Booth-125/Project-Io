#pragma once

// BL-1247 (the ground benchmark): this process's memory as the OS counts it.
// Its own TU so <windows.h>'s `near` / `far` macros never meet a header that
// names job_kind::far (ground_layer.hpp).

namespace io_mem {

/// Bytes. Zero where the platform does not say.
struct process_memory
{
    double working_set      = 0; ///< Resident now.
    double peak_working_set = 0; ///< Most ever resident (the OS tracks it).
    double private_bytes    = 0; ///< Committed private memory now (Task Manager's "Commit size").
    double peak_private     = 0; ///< Most private memory ever committed.
};

process_memory query();

/// CPU time the calling thread has used, ms (kernel + user): a bake's cost
/// that a busy machine's other processes cannot inflate, as wall time can.
/// -1 where the platform does not say.
double thread_cpu_ms();

} // namespace io_mem

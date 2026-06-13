-- Project Io — init.lua
-- Loaded at startup. Confirms Lua is alive and defines global config accessible to C++.

config = {
    sim_hz         = 20,   -- simulation steps per second
    econ_per_sec   = 1,    -- economy ticks per second
}

print(string.format("[Lua] init.lua loaded  sim_hz=%d  econ_per_sec=%d",
    config.sim_hz, config.econ_per_sec))

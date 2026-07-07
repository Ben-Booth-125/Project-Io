-- Project Io — init.lua
-- Loaded at startup. Confirms Lua is alive and defines global config accessible to C++.
-- Calendar/pacing now lives in sim_loop (C++); Lua keeps only the starting speed.

config = {
    default_speed = 2,   -- initial time speed tier (I..V; 0 would start paused)
}

print(string.format("[Lua] init.lua loaded  default_speed=%d", config.default_speed))

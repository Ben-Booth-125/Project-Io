#pragma once

#include <sol/sol.hpp>
#include <string>

/// Owns the sol2 Lua state for the lifetime of the application.
///
/// All sol2 calls that can produce errors use the protected_ variants,
/// per the constraint in TECH_FOUNDATIONS.
class lua_state
{
public:
    lua_state();

    /// Execute a Lua source file.
    ///
    /// @param path Path to the .lua file, relative to the working directory.
    /// @throws std::runtime_error if Lua reports a parse or runtime error.
    void load(const std::string& path);

    /// Direct access to the underlying sol2 state for binding C++ types in later layers.
    sol::state& state() { return m_lua; }

private:
    sol::state m_lua;
};

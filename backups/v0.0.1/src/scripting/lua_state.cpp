#include "lua_state.hpp"
#include <stdexcept>

lua_state::lua_state()
{
    m_lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string);
}

void lua_state::load(const std::string& path)
{
    sol::protected_function_result result =
        m_lua.safe_script_file(path, sol::script_pass_on_error);

    if (!result.valid())
    {
        sol::error err = result;
        throw std::runtime_error("Lua error in '" + path + "': " + err.what());
    }
}

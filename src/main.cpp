#include "core/app.hpp"

#include <string>

int main(int argc, char* argv[])
{
    // Headless visual-verification mode: `ProjectIo --verify [script.lua]` runs a
    // deterministic capture session and exits, instead of the interactive loop.
    // See docs/development/DEVELOPMENT_PRACTICES.md (visual verification).
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--verify")
        {
            const std::string script =
                (i + 1 < argc) ? argv[i + 1] : "scripts/verify/corporation_lens.lua";
            return app{}.run_verify(script);
        }
    }

    return app{}.run();
}

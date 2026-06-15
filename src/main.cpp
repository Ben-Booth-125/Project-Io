#include "core/app.hpp"

#include <string>

int main(int argc, char* argv[])
{
    // Headless visual-verification mode: `ProjectIo --verify [script.lua] [--bless]`
    // runs a deterministic capture session and exits, instead of the interactive
    // loop. With --bless each capture overwrites its golden reference (golden-image
    // diffing, OPENS § Canvas [F3]). See docs/development/DEVELOPMENT_PRACTICES.md.
    bool bless = false;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--bless")
            bless = true;

    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--verify")
        {
            // The script is the next non-flag argument, else the default.
            std::string script = "scripts/verify/corporation_lens.lua";
            if (i + 1 < argc && std::string(argv[i + 1]) != "--bless")
                script = argv[i + 1];
            return app{}.run_verify(script, bless);
        }
    }

    return app{}.run();
}

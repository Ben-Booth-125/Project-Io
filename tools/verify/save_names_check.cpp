// Headless check of the named-save rule (Ben, 2026-11-09; MENU.md § Application /
// system menu): what a save may be called, and what the Load list offers.
//
//   N1  A valid name is accepted and maps to <name>.iosave.
//   N2  Every invalid shape is REFUSED with a reason, never altered: empty, too long,
//       each forbidden character, a control or non-ASCII byte, a leading space, a
//       trailing space or dot, a reserved device name in any case and with an extension.
//   N3  list_saves finds saves newest first, keeps an existing quicksave under the
//       name "quicksave", and ignores other files and names the rule refuses.
//
// Run: save_names_check [scratch_dir]

#include "core/save_names.hpp"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

static int g_failed = 0;

static void check(bool ok, const std::string& what)
{
    std::printf("save_names_check: %s %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok)
        ++g_failed;
}

static void touch(const fs::path& p, int minutes_old)
{
    std::ofstream(p) << "x";
    fs::last_write_time(p, fs::file_time_type::clock::now() - std::chrono::minutes(minutes_old));
}

int main(int argc, char** argv)
{
    // N1
    for (const char* good : {"quicksave", "Second war", "a.b", "run-3_final", "x"})
        check(save_name_error(good) == nullptr, std::string("accepts '") + good + "'");
    check(save_name_error(std::string(save_name_max_len, 'a')) == nullptr, "accepts the longest name");
    check(save_name_path("Second war") == "Second war.iosave", "the name is the file name");

    // N2
    check(save_name_error("") != nullptr, "refuses an empty name");
    check(save_name_error(std::string(save_name_max_len + 1, 'a')) != nullptr, "refuses one byte too long");
    for (const char c : std::string("<>:\"/\\|?*"))
        check(save_name_error(std::string("a") + c + "b") != nullptr, std::string("refuses '") + c + "'");
    check(save_name_error("a\tb") != nullptr, "refuses a control byte");
    check(save_name_error("caf\xC3\xA9") != nullptr, "refuses a non-ASCII name");
    check(save_name_error(" a") != nullptr, "refuses a leading space");
    check(save_name_error("a ") != nullptr, "refuses a trailing space");
    check(save_name_error("a.") != nullptr, "refuses a trailing dot");
    check(save_name_error("..") != nullptr, "refuses '..'");
    for (const char* bad : {"CON", "con", "Nul", "COM1", "lpt9", "aux.old"})
        check(save_name_error(bad) != nullptr, std::string("refuses reserved '") + bad + "'");
    check(save_name_error("CONSOLE") == nullptr, "a reserved name is matched whole");

    // N3
    const fs::path dir = (argc > 1 ? fs::path(argv[1]) : fs::temp_directory_path()) / "save_names_check_dir";
    fs::remove_all(dir);
    fs::create_directories(dir);
    touch(dir / "old.iosave", 30);
    touch(dir / "quicksave.iosave", 20);
    touch(dir / "newest.iosave", 5);
    touch(dir / "notes.txt", 1);
    touch(dir / "CON.iosave.bak", 1);
    const auto list = list_saves(dir);
    check(list.size() == 3, "lists the three saves and nothing else");
    if (list.size() == 3)
    {
        check(list[0].name == "newest" && list[1].name == "quicksave" && list[2].name == "old",
              "newest first, an existing quicksave under its own name");
        check(list[1].path == "quicksave.iosave", "a listing's path is the file beside the save");
    }
    check(list_saves(dir / "missing").empty(), "an unreadable directory lists nothing");
    fs::remove_all(dir);

    if (g_failed > 0)
    {
        std::printf("save_names_check: %d failed\n", g_failed);
        return 1;
    }
    std::printf("save_names_check: OK\n");
    return 0;
}

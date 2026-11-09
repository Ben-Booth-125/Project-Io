#pragma once

/// Named saves (Ben, 2026-11-09; MENU.md § Application / system menu). A save's name
/// IS its file name: `<name>.iosave` beside the executable, where F5's `quicksave`
/// already lives, so nothing is added to the save payload and every existing quicksave
/// lists and loads as a save named "quicksave".
///
/// A name the file system could not hold exactly is REJECTED with the reason, never
/// altered: the name the player typed is the name the save is listed under.
///
/// Header-only and free of ImGui and SDL, so tools/verify/save_names_check.cpp can
/// drive the rule with no window.

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

/// Default extension for a save file. One place, so the CLI, the quick-save
/// binding, the named-save rule and the verify API cannot disagree about it.
inline constexpr const char* save_game_extension = ".iosave";

/// The longest name a save may carry, in bytes.
inline constexpr std::size_t save_name_max_len = 64;

/// Why `name` cannot be a save's name, or nullptr when it can. Plain words, drawn
/// under the field as the player types.
///
/// ASCII only, deliberately: the save path travels as a narrow string to an ofstream,
/// and on Windows that reads a UTF-8 name through the system code page, so a non-ASCII
/// name would land on disk as different characters. That is the silent alteration this
/// rule exists to refuse.
inline const char* save_name_error(std::string_view name)
{
    if (name.empty())
        return "Type a name for this save.";
    if (name.size() > save_name_max_len)
        return "That name is too long.";
    for (const char ch : name)
    {
        const auto c = static_cast<unsigned char>(ch);
        if (c < 0x20 || c >= 0x7F)
            return "Use plain letters, numbers and spaces.";
        if (std::string_view("<>:\"/\\|?*").find(ch) != std::string_view::npos)
            return "A name cannot hold any of < > : \" / \\ | ? *";
    }
    if (name.front() == ' ')
        return "A name cannot start with a space.";
    if (name.back() == ' ' || name.back() == '.')
        return "A name cannot end with a space or a full stop.";

    // The device names Windows reserves, whatever the extension after them.
    std::string upper(name.substr(0, name.find('.')));
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    static constexpr std::string_view reserved[] = {
        "CON",  "PRN",  "AUX",  "NUL",  "COM1", "COM2", "COM3", "COM4", "COM5", "COM6",
        "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7",
        "LPT8", "LPT9"};
    for (const std::string_view r : reserved)
        if (upper == r)
            return "Windows reserves that name.";
    return nullptr;
}

/// The file a named save lives in. Only for a name `save_name_error` accepts.
inline std::string save_name_path(std::string_view name)
{
    return std::string(name) + save_game_extension;
}

/// One save on disk, as the Load list shows it.
struct save_listing
{
    std::string name;
    std::string path;
};

/// Every save in `dir`, newest first (ties by name, so the order is stable). A file
/// whose stem this rule would refuse is skipped: the list only offers what a name can
/// say. An unreadable directory lists nothing.
inline std::vector<save_listing> list_saves(const std::filesystem::path& dir = ".")
{
    struct row
    {
        save_listing                    listing;
        std::filesystem::file_time_type when;
    };
    std::vector<row> rows;
    std::error_code  ec;
    for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
    {
        std::error_code entry_ec;
        if (!it->is_regular_file(entry_ec) || entry_ec)
            continue;
        try
        {
            const std::filesystem::path& p = it->path();
            if (p.extension().string() != save_game_extension)
                continue;
            std::string name = p.stem().string();
            if (save_name_error(name) != nullptr)
                continue;
            const auto when = it->last_write_time(entry_ec);
            if (entry_ec)
                continue;
            rows.push_back({{name, p.filename().string()}, when});
        }
        catch (const std::system_error&)
        {
            continue; // a file name the narrow encoding cannot hold
        }
    }
    std::sort(rows.begin(), rows.end(), [](const row& a, const row& b) {
        if (a.when != b.when)
            return a.when > b.when;
        return a.listing.name < b.listing.name;
    });
    std::vector<save_listing> out;
    out.reserve(rows.size());
    for (row& r : rows)
        out.push_back(std::move(r.listing));
    return out;
}

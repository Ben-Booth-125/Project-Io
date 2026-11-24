#pragma once
// The audio mixer (docs/ui/SOUND.md §§ 6-9): one SDL3 playback stream, pre-rendered cue
// buffers, a voice cap with priority drop, the wall-clock merge window, the bed and its duck,
// and the player's volume groups.
//
// Audio is a view. Nothing here reads or writes the world; the main thread only names cues.
// With no device (headless, a failed open) requests are still recorded, so a check can ask
// which cues a frame fired without opening the speakers.

#include "synth.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace audio
{

struct volumes
{
    float master = 0.8f;
    float ui     = 0.8f;
    float event  = 0.8f;
    float bed    = 0.6f;
    bool  mute   = false;
};

class mixer
{
public:
    mixer();
    ~mixer();
    mixer(const mixer&) = delete;
    mixer& operator=(const mixer&) = delete;

    /// Load the table and render every cue. Does not open a device. False: silent, `error` says why.
    bool load(const std::string& table_path, std::string& error);

    /// Open the default playback device. False: the game runs silent (logged once by the caller).
    bool open_device(std::string& error);
    void close_device();
    bool has_device() const;

    /// Name a cue (main thread). Unknown names are ignored. Repeats of one cue inside the
    /// merge window play once (§ 6). `now_ms` is wall-clock milliseconds.
    void request(std::string_view cue, std::uint64_t now_ms);

    void set_volumes(const volumes& v);
    void set_focus(bool focused);   ///< quiet on focus loss (§ 8)
    void set_bed(bool playing);     ///< the bed plays in game, not at the menu (§ 7)

    /// Cues accepted since the last call (merged repeats excluded); clears the list.
    std::vector<std::string> take_fired();

    static constexpr std::uint64_t k_merge_window_ms = 250; ///< calibration (SOUND.md § 6)
    static constexpr int           k_voice_cap       = 8;

private:
    struct impl;
    std::unique_ptr<impl> m;
};

} // namespace audio

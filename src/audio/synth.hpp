#pragma once
// The cue synthesiser (docs/ui/SOUND.md § 2). Pure: no SDL, no world, no I/O beyond the cue
// table loader. Every sound in the game is rendered here from scripts/sounds.lua.
//
// The rendering mirrors tools/session/sound_page.html (the listening page); change both
// together, so what Ben hears on the page is what the game plays.

#include <cstdint>
#include <string>
#include <vector>

namespace audio
{

inline constexpr int k_sample_rate = 48000;

enum class wave : std::uint8_t { sine, tri, square, noise };

struct note
{
    wave  shape   = wave::sine;
    float f       = 440.f;  ///< start pitch, Hz
    float f1      = 440.f;  ///< end pitch, Hz (exponential glide)
    float at      = 0.f;    ///< start offset inside the cue, s
    float a       = 0.005f; ///< attack, s
    float h       = 0.f;    ///< hold, s
    float r       = 0.1f;   ///< release to -60 dB, s
    float g       = 1.f;    ///< gain 0..1
    float lp      = 0.f;    ///< one-pole low-pass cutoff, Hz; 0 = none
    float lp1     = 0.f;    ///< end cutoff, Hz — an exponential sweep across the note; 0 = lp
};

enum class cue_group : std::uint8_t { ui, event };

struct cue_def
{
    std::string       name;
    cue_group         group    = cue_group::ui;
    int               priority = 1;
    float             gain     = 0.5f;
    std::vector<note> notes;
};

struct bed_partial { float f = 55.f, g = 0.5f, lfo = 0.03f, depth = 0.5f; };

struct bed_def
{
    float                    gain = 0.f;
    std::vector<bed_partial> partials;
    float                    noise_g  = 0.f;
    float                    noise_lp = 300.f;
    float                    duck_depth = 0.5f;
    float                    duck_a     = 0.03f;
    float                    duck_r     = 0.6f;
};

struct cue_table
{
    std::vector<cue_def> cues;
    bed_def              bed;
};

/// Read scripts/sounds.lua. Returns false (and an empty table) on any fault, with the reason
/// in `error`; the game then runs silent. Uses its own private Lua state.
bool load_cue_table(const std::string& path, cue_table& out, std::string& error);

/// Render one cue to mono float samples at k_sample_rate, already scaled by the cue's gain.
std::vector<float> render_cue(const cue_def& cue);

/// The bed's partials and noise at sample index `k` of a running stream. `noise_state` and
/// `noise_y` carry the noise generator between calls.
float bed_sample(const bed_def& bed, std::uint64_t k, std::uint32_t& noise_state, float& noise_y);

} // namespace audio

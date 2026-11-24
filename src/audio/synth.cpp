#include "synth.hpp"

#include <sol/sol.hpp>

#include <algorithm>
#include <cmath>

namespace audio
{
namespace
{
constexpr double k_two_pi = 6.283185307179586;

bool parse_wave(const std::string& s, wave& w)
{
    if (s == "sine")   { w = wave::sine;   return true; }
    if (s == "tri")    { w = wave::tri;    return true; }
    if (s == "square") { w = wave::square; return true; }
    if (s == "noise")  { w = wave::noise;  return true; }
    return false;
}

float num(const sol::table& t, const char* key, float fallback)
{
    sol::optional<double> v = t[key];
    return v ? static_cast<float>(*v) : fallback;
}

/// One step of the shared noise generator (an LCG; the page uses the same constants).
inline float next_noise(std::uint32_t& state)
{
    state = state * 1664525u + 1013904223u;
    return static_cast<float>(state / 2147483648.0 - 1.0);
}
} // namespace

bool load_cue_table(const std::string& path, cue_table& out, std::string& error)
{
    out = {};
    sol::state lua;
    lua.open_libraries(sol::lib::base);
    sol::protected_function_result res = lua.safe_script_file(path, sol::script_pass_on_error);
    if (!res.valid())
    {
        sol::error e = res;
        error = e.what();
        return false;
    }
    sol::optional<sol::table> root = res.get<sol::optional<sol::table>>();
    if (!root) { error = "sounds.lua must return a table"; return false; }

    sol::optional<sol::table> cues = (*root)["cues"];
    if (!cues) { error = "sounds.lua: no `cues` table"; return false; }
    for (const auto& [key, value] : *cues)
    {
        if (!key.is<std::string>() || !value.is<sol::table>()) continue;
        sol::table c = value.as<sol::table>();
        cue_def def;
        def.name     = key.as<std::string>();
        def.group    = c.get_or<std::string>("group", "ui") == "event" ? cue_group::event : cue_group::ui;
        def.priority = static_cast<int>(num(c, "priority", 1.f));
        def.gain     = std::clamp(num(c, "gain", 0.5f), 0.f, 1.f);
        sol::optional<sol::table> notes = c["notes"];
        if (!notes) { error = def.name + ": no notes"; return false; }
        for (const auto& [nk, nv] : *notes)
        {
            (void)nk;
            if (!nv.is<sol::table>()) continue;
            sol::table n = nv.as<sol::table>();
            note o;
            if (!parse_wave(n.get_or<std::string>("wave", ""), o.shape))
            {
                error = def.name + ": unknown wave";
                return false;
            }
            o.f  = num(n, "f", 440.f);
            o.f1 = num(n, "f1", o.f);
            o.at = std::max(0.f, num(n, "at", 0.f));
            o.a  = std::max(0.0005f, num(n, "a", 0.005f));
            o.h  = std::max(0.f, num(n, "h", 0.f));
            o.r  = std::max(0.005f, num(n, "r", 0.1f));
            o.g  = std::clamp(num(n, "g", 1.f), 0.f, 1.f);
            o.lp = std::max(0.f, num(n, "lp", 0.f));
            o.lp1 = o.lp > 0.f ? std::max(20.f, num(n, "lp1", o.lp)) : 0.f;
            def.notes.push_back(o);
        }
        out.cues.push_back(std::move(def));
    }
    // Lua's pairs order is unspecified; sort so the table is the same every run.
    std::sort(out.cues.begin(), out.cues.end(),
              [](const cue_def& x, const cue_def& y) { return x.name < y.name; });

    if (sol::optional<sol::table> bed = (*root)["bed"])
    {
        out.bed.gain = std::clamp(num(*bed, "gain", 0.f), 0.f, 1.f);
        if (sol::optional<sol::table> ps = (*bed)["partials"])
            for (const auto& [pk, pv] : *ps)
            {
                (void)pk;
                if (!pv.is<sol::table>()) continue;
                sol::table p = pv.as<sol::table>();
                out.bed.partials.push_back({num(p, "f", 55.f), num(p, "g", 0.5f),
                                            num(p, "lfo", 0.03f), num(p, "depth", 0.5f)});
            }
        if (sol::optional<sol::table> nz = (*bed)["noise"])
        {
            out.bed.noise_g  = num(*nz, "g", 0.f);
            out.bed.noise_lp = num(*nz, "lp", 300.f);
        }
        if (sol::optional<sol::table> d = (*bed)["duck"])
        {
            out.bed.duck_depth = std::clamp(num(*d, "depth", 0.5f), 0.f, 1.f);
            out.bed.duck_a     = std::max(0.001f, num(*d, "a", 0.03f));
            out.bed.duck_r     = std::max(0.001f, num(*d, "r", 0.6f));
        }
    }
    return true;
}

std::vector<float> render_cue(const cue_def& cue)
{
    double len = 0.0;
    for (const note& n : cue.notes) len = std::max(len, double(n.at + n.a + n.h + n.r));
    std::vector<float> out(static_cast<std::size_t>(std::ceil(len * k_sample_rate)) + 1, 0.f);

    for (std::size_t idx = 0; idx < cue.notes.size(); ++idx)
    {
        const note& n   = cue.notes[idx];
        const auto start = static_cast<std::size_t>(std::lround(n.at * k_sample_rate));
        const double dur = double(n.a) + n.h + n.r;
        const auto count = static_cast<std::size_t>(std::lround(dur * k_sample_rate));
        const bool   sweep    = n.lp > 0.f && n.lp1 > 0.f && n.lp1 != n.lp;
        const double lp_ratio = sweep ? double(n.lp1) / n.lp : 1.0;
        double alpha = n.lp > 0.f ? 1.0 - std::exp(-k_two_pi * n.lp / k_sample_rate) : 1.0;
        const double ratio = n.f > 0.f ? double(n.f1) / n.f : 1.0;
        double phase = 0.0, y = 0.0;
        std::uint32_t seed = 0x9E3779B9u ^ static_cast<std::uint32_t>((idx + 1) * 2654435761u);
        for (std::size_t k = 0; k < count && start + k < out.size(); ++k)
        {
            const double t = double(k) / k_sample_rate;
            const double env = t < n.a ? t / n.a
                             : t < double(n.a) + n.h ? 1.0
                             : std::exp(-6.9078 * (t - n.a - n.h) / n.r);
            double v;
            if (n.shape == wave::noise)
                v = next_noise(seed);
            else
            {
                const double freq = n.f * std::pow(ratio, t / dur);
                phase += k_two_pi * freq / k_sample_rate;
                if (phase > k_two_pi) phase -= k_two_pi;
                const double s = std::sin(phase);
                v = n.shape == wave::sine ? s
                  : n.shape == wave::tri  ? (2.0 / 3.141592653589793) * std::asin(s)
                  : (s >= 0.0 ? 1.0 : -1.0);
            }
            if (sweep)
                alpha = 1.0 - std::exp(-k_two_pi * n.lp * std::pow(lp_ratio, t / dur) / k_sample_rate);
            y += alpha * (v - y);
            out[start + k] += static_cast<float>(env * n.g * y * cue.gain);
        }
    }
    return out;
}

float bed_sample(const bed_def& bed, std::uint64_t k, std::uint32_t& noise_state, float& noise_y)
{
    const double t = double(k) / k_sample_rate;
    double s = 0.0;
    for (std::size_t i = 0; i < bed.partials.size(); ++i)
    {
        const bed_partial& p = bed.partials[i];
        const double lvl = 1.0 - p.depth * 0.5 * (1.0 + std::sin(k_two_pi * p.lfo * t + double(i) * 1.7));
        s += p.g * lvl * std::sin(k_two_pi * std::fmod(p.f * t, 1.0));
    }
    const float alpha = 1.f - std::exp(static_cast<float>(-k_two_pi * bed.noise_lp / k_sample_rate));
    noise_y += alpha * (next_noise(noise_state) - noise_y);
    return static_cast<float>((s + bed.noise_g * noise_y) * bed.gain);
}

} // namespace audio

#include "mixer.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <mutex>
#include <unordered_map>

namespace audio
{

struct mixer::impl
{
    struct rendered { cue_def def; std::vector<float> pcm; };
    struct voice    { const rendered* cue = nullptr; std::size_t pos = 0; };

    // Fixed after load(); read by both threads without a lock.
    std::vector<rendered>                      cues;
    std::unordered_map<std::string, std::size_t> index;
    bed_def                                    bed;

    // Main thread only.
    std::unordered_map<std::size_t, std::uint64_t> last_request_ms;
    std::vector<std::string>                   fired;

    // Shared with the device thread under `lock`.
    std::mutex             lock;
    std::vector<std::size_t> pending;
    volumes                vol;
    bool                   focused = true;
    bool                   bed_on  = false;

    // Device thread only.
    std::vector<voice> voices;
    std::vector<float> scratch;
    std::uint64_t      bed_k = 0;
    std::uint32_t      bed_noise = 12345u;
    float              bed_noise_y = 0.f;
    float              bed_level = 0.f;   // smoothed on/off and focus
    float              duck = 0.f;        // 0 = none, 1 = full duck
    float              master_smooth = 0.f;

    SDL_AudioStream* stream = nullptr;

    static void SDLCALL feed(void* user, SDL_AudioStream* s, int additional, int /*total*/)
    {
        auto* self = static_cast<impl*>(user);
        const int frames = additional / int(sizeof(float));
        if (frames <= 0) return;
        self->mix(frames);
        SDL_PutAudioStreamData(s, self->scratch.data(), frames * int(sizeof(float)));
    }

    void mix(int frames)
    {
        volumes v;
        bool focus, bed_wanted;
        {
            std::lock_guard<std::mutex> g(lock);
            for (std::size_t i : pending)
            {
                voices.push_back({&cues[i], 0});
                if (int(voices.size()) > k_voice_cap)
                {
                    // Past the cap the lowest-priority voice drops; among equals, the oldest.
                    auto lowest = std::min_element(voices.begin(), voices.end(),
                        [](const voice& a, const voice& b) { return a.cue->def.priority < b.cue->def.priority; });
                    voices.erase(lowest);
                }
            }
            if (!pending.empty()) duck = 1.f;
            pending.clear();
            v = vol; focus = focused; bed_wanted = bed_on;
        }
        scratch.assign(std::size_t(frames), 0.f);

        const float master_target = v.mute ? 0.f : v.master * (focus ? 1.f : 0.25f);
        const float bed_target    = bed_wanted ? 1.f : 0.f;
        const float smooth        = 1.f - std::exp(-1.f / (0.05f * k_sample_rate));
        const float duck_release  = std::exp(-1.f / (std::max(0.001f, bed.duck_r) * k_sample_rate));

        for (voice& vc : voices)
        {
            const float group = vc.cue->def.group == cue_group::ui ? v.ui : v.event;
            const std::vector<float>& pcm = vc.cue->pcm;
            for (int k = 0; k < frames && vc.pos < pcm.size(); ++k, ++vc.pos)
                scratch[std::size_t(k)] += pcm[vc.pos] * group;
        }
        voices.erase(std::remove_if(voices.begin(), voices.end(),
                         [](const voice& vc) { return vc.pos >= vc.cue->pcm.size(); }),
                     voices.end());

        for (int k = 0; k < frames; ++k)
        {
            bed_level     += smooth * (bed_target - bed_level);
            master_smooth += smooth * (master_target - master_smooth);
            float s = scratch[std::size_t(k)];
            if (bed.gain > 0.f && bed_level > 1e-4f)
            {
                const float b = bed_sample(bed, bed_k++, bed_noise, bed_noise_y);
                s += b * v.bed * bed_level * (1.f - bed.duck_depth * duck);
            }
            duck *= duck_release;
            // Soft limiter: transparent at normal levels, never a hard clip.
            scratch[std::size_t(k)] = std::tanh(s * master_smooth);
        }
    }
};

mixer::mixer() : m(std::make_unique<impl>()) {}
mixer::~mixer() { close_device(); }

bool mixer::load(const std::string& table_path, std::string& error)
{
    cue_table t;
    if (!load_cue_table(table_path, t, error)) return false;
    m->cues.clear();
    m->index.clear();
    for (cue_def& c : t.cues)
    {
        std::vector<float> pcm = render_cue(c);
        m->index[c.name] = m->cues.size();
        m->cues.push_back({std::move(c), std::move(pcm)});
    }
    m->bed = t.bed;
    return true;
}

bool mixer::open_device(std::string& error)
{
    if (m->stream) return true;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) { error = SDL_GetError(); return false; }
    SDL_AudioSpec spec{};
    spec.format   = SDL_AUDIO_F32;
    spec.channels = 1;
    spec.freq     = k_sample_rate;
    m->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &impl::feed, m.get());
    if (!m->stream)
    {
        error = SDL_GetError();
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }
    SDL_ResumeAudioStreamDevice(m->stream);
    return true;
}

void mixer::close_device()
{
    if (!m || !m->stream) return;
    SDL_DestroyAudioStream(m->stream); // stops the callback before returning
    m->stream = nullptr;
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

bool mixer::has_device() const { return m->stream != nullptr; }

void mixer::request(std::string_view cue, std::uint64_t now_ms)
{
    auto it = m->index.find(std::string(cue));
    if (it == m->index.end()) return;
    auto last = m->last_request_ms.find(it->second);
    if (last != m->last_request_ms.end() && now_ms - last->second < k_merge_window_ms) return;
    m->last_request_ms[it->second] = now_ms;
    m->fired.emplace_back(cue);
    if (!m->stream) return;
    std::lock_guard<std::mutex> g(m->lock);
    m->pending.push_back(it->second);
}

void mixer::set_volumes(const volumes& v) { std::lock_guard<std::mutex> g(m->lock); m->vol = v; }
void mixer::set_focus(bool f)            { std::lock_guard<std::mutex> g(m->lock); m->focused = f; }
void mixer::set_bed(bool on)             { std::lock_guard<std::mutex> g(m->lock); m->bed_on = on; }

std::vector<std::string> mixer::take_fired()
{
    std::vector<std::string> out;
    out.swap(m->fired);
    return out;
}

} // namespace audio

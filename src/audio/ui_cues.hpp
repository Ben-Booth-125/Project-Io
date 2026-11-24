#pragma once
// A press the UI code alone can name (docs/ui/SOUND.md § 3): a tab switch inside a ledger, a
// refused command. The UI notes the cue; the cue watcher drains the list once a frame. No SDL,
// no mixer — a UI translation unit includes only this.

#include <string>
#include <vector>

namespace audio
{

inline std::vector<std::string>& ui_cue_notes()
{
    static std::vector<std::string> notes;
    return notes;
}

/// Note a UI cue by name (scripts/sounds.lua). Cheap; harmless when sound is off.
/// Under --verify nothing drains the list, so it is kept short.
inline void note_ui(const char* cue)
{
    auto& notes = ui_cue_notes();
    if (notes.size() >= 32) notes.clear();
    notes.emplace_back(cue);
}

} // namespace audio

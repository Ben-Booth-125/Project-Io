#include "text_fit.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace ui {

namespace {

/// Container name for the report — matches LAYOUT.md's container table rows.
const char* box_name(text_box b)
{
    switch (b)
    {
        case text_box::foldout_column: return "1 fold-out column";
        case text_box::canvas_legend:  return "2 canvas legend";
        case text_box::selection:      return "3 selection band";
        case text_box::header_strip:   return "4 header strip";
        case text_box::time_panel:     return "5 time panel";
        case text_box::hover_card:     return "6 hover card";
        case text_box::lens_bar:       return "7 lens bar";
        case text_box::nav_rail:       return "8 nav rail";
        case text_box::table_cell:     return "9 table cell";
        case text_box::chart_plot:     return "10 chart plot";
    }
    return "?";
}

const char* sev_name(overflow_severity s)
{
    switch (s)
    {
        case overflow_severity::elided:     return "elided";
        case overflow_severity::clipped:    return "clipped";
        case overflow_severity::unfittable: return "unfittable";
    }
    return "?";
}

bool                       g_recording = false;
std::string                g_frame;
std::vector<text_overflow> g_records;

/// Every text this ledger MEASURED, whatever the outcome (BL-714). Counted
/// separately from g_records because a record is an overflow EVENT and this is
/// a LOOK — see text_fit.hpp @ overflow_observations for why the distinction is
/// the difference between a real pass and a vacuous one.
std::size_t                g_observations = 0;

/// Record one overflow, deduping on (site, text) — a sixty-frame run must not
/// flood the report. The stored record keeps the first frame label and the
/// worst severity seen.
void record(text_box box, const char* site, const char* text,
            float needed, float available, overflow_severity sev)
{
    if (!g_recording)
        return;
    for (text_overflow& r : g_records)
    {
        if (std::strcmp(r.site, site) == 0 && r.text == text)
        {
            if (static_cast<int>(sev) > static_cast<int>(r.sev))
                r.sev = sev;
            return;
        }
    }
    text_overflow r;
    r.box       = box;
    r.site      = site;
    r.text      = text;
    r.frame     = g_frame;
    r.needed    = needed;
    r.available = available;
    r.sev       = sev;
    g_records.push_back(std::move(r));
}

/// Widest prefix of @p text that fits @p max_w with an ellipsis appended, or an
/// empty string when even the ellipsis alone does not fit.
std::string elide(const char* text, float max_w)
{
    const float dots = ImGui::CalcTextSize("...").x;
    if (dots > max_w)
        return std::string();
    std::string s = text;
    while (!s.empty() && ImGui::CalcTextSize(s.c_str()).x + dots > max_w)
        s.pop_back();
    return s + "...";
}

/// Greedy word wrap of @p text into at most @p max_lines lines of @p max_w px.
/// A word wider than the whole box is hard-broken by characters — a wrapping box's
/// only clip case — rather than left to overrun. @p overran is set when text
/// remained after the last allowed line, which is the one case that still elides.
std::vector<std::string> wrap_to_lines(const char* text, float max_w, int max_lines,
                                       bool& overran, std::string* tail_from_last = nullptr)
{
    overran = false;
    std::size_t last_start = 0;
    std::vector<std::string> lines;
    if (text == nullptr || text[0] == '\0' || max_lines <= 0)
        return lines;
    const std::string s = text;
    std::size_t       i = 0;
    while (i < s.size() && static_cast<int>(lines.size()) < max_lines)
    {
        while (i < s.size() && s[i] == ' ')
            ++i;
        if (i >= s.size())
            break;
        const std::size_t line_start = i;
        std::string       line;
        while (i < s.size())
        {
            // The next word, with the space that joins it to what we have.
            std::size_t j = s.find(' ', i);
            if (j == std::string::npos)
                j = s.size();
            const std::string word = s.substr(i, j - i);
            const std::string with = line.empty() ? word : line + " " + word;
            if (max_w > 0.0f && ImGui::CalcTextSize(with.c_str()).x > max_w)
            {
                if (!line.empty())
                    break; // the word starts the next line
                // A single word wider than the box: hard-break it by characters so
                // the tail survives onto the next line instead of being clipped.
                std::string part;
                while (i < j)
                {
                    const std::string grown = part + s[i];
                    if (!part.empty() && ImGui::CalcTextSize(grown.c_str()).x > max_w)
                        break;
                    part = grown;
                    ++i;
                }
                line = part;
                break;
            }
            line = with;
            i    = j;
        }
        if (line.empty())
            break; // nothing fits at all; the caller's record says so
        last_start = line_start;
        lines.push_back(std::move(line));
    }
    while (i < s.size() && s[i] == ' ')
        ++i;
    overran = (i < s.size());
    // WHAT THE LAST LINE SHOULD CARRY WHEN THE BUDGET RAN OUT. Eliding the last
    // line as it was drops the words that fitted on it; eliding the REMAINDER
    // from where that line began keeps as many characters of the tail as the
    // column holds, which is strictly more of the string for the same pixels.
    if (overran && tail_from_last != nullptr && !lines.empty())
        *tail_from_last = s.substr(last_start);
    return lines;
}

} // namespace

int fit_wrapped_text(text_box box, const char* site, const char* text, float max_w,
                     int max_lines, bool disabled, bool* elided_out)
{
    if (g_recording) ++g_observations; // BL-714: the ledger looked here
    if (elided_out != nullptr)
        *elided_out = false;
    if (text == nullptr || text[0] == '\0')
    {
        ImGui::Dummy({0.0f, ImGui::GetTextLineHeight()});
        return 0;
    }
    bool        overran = false;
    std::string tail;
    const auto  lines   = wrap_to_lines(text, std::max(max_w, 0.0f),
                                        std::max(max_lines, 1), overran, &tail);
    if (lines.empty())
    {
        // Not even one character fits: keep the slot, keep the value one hover
        // away, and record the hard failure — the container must reflow.
        ImGui::Dummy({0.0f, ImGui::GetTextLineHeight()});
        ImGui::SetItemTooltip("%s", text);
        record(box, site, text, ImGui::CalcTextSize(text).x, max_w,
               overflow_severity::clipped);
        if (elided_out != nullptr)
            *elided_out = true;
        return 0;
    }
    // ZERO SPACING BETWEEN THE LINES. The caller's row is a budget of N text
    // lines; ImGui's default item spacing between two text items would push the
    // second line out of it, and the row would clip exactly where it was meant
    // to wrap.
    const ImVec2 sp = ImGui::GetStyle().ItemSpacing;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {sp.x, 0.0f});
    std::string last = lines.back();
    if (overran)
    {
        // The budget could not hold it: the LAST line elides, which is the only
        // place this helper ever elides, and the full string goes on a tooltip.
        const std::string e = elide(tail.empty() ? last.c_str() : tail.c_str(),
                                    std::max(max_w, 0.0f));
        if (!e.empty())
            last = e;
    }
    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        const char* draw = (i + 1 == lines.size()) ? last.c_str() : lines[i].c_str();
        if (disabled)
            ImGui::TextDisabled("%s", draw);
        else
            ImGui::TextUnformatted(draw);
    }
    ImGui::PopStyleVar();
    if (overran)
    {
        ImGui::SetItemTooltip("%s", text);
        record(box, site, text, ImGui::CalcTextSize(text).x, max_w,
               overflow_severity::elided);
        if (elided_out != nullptr)
            *elided_out = true;
    }
    return static_cast<int>(lines.size());
}

bool fit_text(text_box box, const char* site, const char* text, float max_w,
              bool disabled)
{
    if (g_recording) ++g_observations; // BL-714: the ledger looked here
    const float needed = ImGui::CalcTextSize(text).x;
    if (max_w > 0.0f && needed <= max_w)
    {
        if (disabled)
            ImGui::TextDisabled("%s", text);
        else
            ImGui::TextUnformatted(text);
        return false;
    }

    const std::string s = elide(text, std::max(max_w, 0.0f));
    if (s.empty())
    {
        // Even the ellipsis does not fit: draw nothing, keep the layout slot,
        // keep the value one hover away, and record the hard failure.
        ImGui::Dummy({0.0f, ImGui::GetTextLineHeight()});
        ImGui::SetItemTooltip("%s", text);
        record(box, site, text, needed, max_w, overflow_severity::clipped);
        return true;
    }
    if (disabled)
        ImGui::TextDisabled("%s", s.c_str());
    else
        ImGui::TextUnformatted(s.c_str());
    ImGui::SetItemTooltip("%s", text);
    record(box, site, text, needed, max_w, overflow_severity::elided);
    return true;
}

void wrap_text(text_box box, const char* site, const char* text, float max_w)
{
    // Wrap policy never elides (LAYOUT.md: wrap vs guaranteed-fit is a binary
    // choice per container). The only way a wrapping box still clips is a
    // single unbreakable token wider than the box — detect and record that.
    if (g_recording) ++g_observations; // BL-714: the ledger looked here
    if (g_recording && max_w > 0.0f)
    {
        const char* p = text;
        while (*p)
        {
            const char* tok = p;
            while (*p && *p != ' ' && *p != '\n')
                ++p;
            if (p != tok && ImGui::CalcTextSize(tok, p).x > max_w)
            {
                record(box, site, text, ImGui::CalcTextSize(tok, p).x, max_w,
                       overflow_severity::unfittable);
                break;
            }
            while (*p == ' ' || *p == '\n')
                ++p;
        }
    }
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + max_w);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
}

float add_fit_text(ImDrawList* dl, text_box box, const char* site,
                   ImVec2 pos, float max_w, ImU32 col, const char* text,
                   text_align align)
{
    if (g_recording) ++g_observations; // BL-714: the ledger looked here
    const float needed = ImGui::CalcTextSize(text).x;
    std::string storage;
    const char* draw = text;
    float       w    = needed;
    if (max_w > 0.0f && needed > max_w)
    {
        storage = elide(text, max_w);
        if (storage.empty())
        {
            // No ImGui item exists to hang a tooltip on — a clipped record here
            // means the CONTAINER must reflow; that is the fix, not a hack.
            record(box, site, text, needed, max_w, overflow_severity::clipped);
            return 0.0f;
        }
        draw = storage.c_str();
        w    = ImGui::CalcTextSize(draw).x;
        record(box, site, text, needed, max_w, overflow_severity::elided);
    }
    ImVec2 at = pos;
    if (align == text_align::right)
        at.x -= w;
    else if (align == text_align::centre)
        at.x -= w * 0.5f;
    dl->AddText(at, col, draw);
    return w;
}

float fit_width(const char* text)
{
    return ImGui::CalcTextSize(text).x;
}

std::string fitted(const char* text, float max_w)
{
    if (max_w <= 0.0f || ImGui::CalcTextSize(text).x <= max_w)
        return text;
    return elide(text, max_w);
}

void set_overflow_recording(bool on)      { g_recording = on; }
bool overflow_recording()                 { return g_recording; }
void set_overflow_frame(const std::string& label) { g_frame = label; }
const std::vector<text_overflow>& overflows() { return g_records; }
void clear_overflows()                    { g_records.clear(); g_observations = 0; }
std::size_t overflow_observations()       { return g_observations; }

std::size_t overflow_failures()
{
    std::size_t n = 0;
    for (const text_overflow& r : g_records)
        if (r.sev != overflow_severity::elided)
            ++n;
    return n;
}

std::size_t write_overflow_report(const char* path)
{
    std::FILE* f = std::fopen(path, "w");
    if (!f)
        return overflow_failures();
    for (const text_overflow& r : g_records)
        std::fprintf(f, "%s  [%s] %s  frame=%s  needed=%.0fpx available=%.0fpx\n  \"%s\"\n",
                     sev_name(r.sev), box_name(r.box), r.site,
                     r.frame.empty() ? "-" : r.frame.c_str(),
                     static_cast<double>(r.needed), static_cast<double>(r.available),
                     r.text.c_str());
    std::fclose(f);
    return overflow_failures();
}

} // namespace ui

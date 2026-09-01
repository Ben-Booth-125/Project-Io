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

} // namespace

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

#include "front_view.hpp"

#include "presentation.hpp"

#include "world/hex_neighbors.hpp"
#include "world/stance.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ui {

namespace {

struct fronts_cache
{
    const world*            w         = nullptr;
    entity_id               nation    = null_entity;
    int                     day_tick  = -1;
    std::size_t             territory = ~std::size_t{0};
    std::vector<front_view> views;
};

entity_id nation_of_tile(const world& w, entity_id tile)
{
    const auto it = w.tile_to_nation.find(tile);
    return it != w.tile_to_nation.end() ? it->second : null_entity;
}

// The span's mean grid position — what "north" and "south" are said of.
void span_centre(const world& w, const front& f, float& col, float& row)
{
    col = row = 0.0f;
    int n = 0;
    for (const entity_id t : f.span)
        if (const auto tit = w.tiles.find(t); tit != w.tiles.end())
        {
            col += static_cast<float>(tit->second.grid_x);
            row += static_cast<float>(tit->second.grid_y);
            ++n;
        }
    if (n > 0)
    {
        col /= static_cast<float>(n);
        row /= static_cast<float>(n);
    }
}

void build_views(const world& w, entity_id nation, std::vector<front_view>& out)
{
    out.clear();
    const std::vector<front> fronts = derive_fronts(w, nation);
    out.reserve(fronts.size());

    for (const front& f : fronts)
    {
        front_view v;
        v.f = f;
        for (const entity_id t : f.span)
        {
            const auto tit = w.tiles.find(t);
            if (tit == w.tiles.end())
                continue;
            const tile_component& tc = tit->second;
            v.body = tc.body;
            const auto bit = w.bodies.find(tc.body);
            const auto rit = w.body_tile_index.find(tc.body);
            if (bit == w.bodies.end() || rit == w.body_tile_index.end())
                continue;
            const int gw = std::max(1, bit->second.grid_width);
            const int gh = bit->second.grid_height;
            const std::vector<entity_id>& raster = rit->second;
            if (raster.size() != static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh))
                continue;
            for (int side = 0; side < 6; ++side)
            {
                const hex_neighbors::coord nc = hex_neighbors::neighbour(tc.grid_x, tc.grid_y, side);
                if (nc.gy < 0 || nc.gy >= gh)
                    continue;
                const int ncol = ((nc.gx % gw) + gw) % gw; // the east-west cylinder
                const entity_id nb = raster[static_cast<std::size_t>(nc.gy) * gw + ncol];
                if (nb == null_entity || nation_of_tile(w, nb) != f.neighbour)
                    continue;
                // Keep the UNWRAPPED neighbour column: the canvas draws the edge
                // from the span tile's own copy, so the far side must sit beside it.
                v.edges.push_back({ tc.grid_x, tc.grid_y, nc.gx, nc.gy });
            }
        }
        out.push_back(std::move(v));
    }

    // Labels: the neighbour's name and the span's length; a neighbour met on more
    // than one front is told apart by compass (two) or by its place (three or more).
    for (std::size_t i = 0; i < out.size(); ++i)
    {
        front_view& v = out[i];
        const char* nb_name = "\xe2\x80\x94";
        if (const auto nit = w.nations.find(v.f.neighbour); nit != w.nations.end()
            && !nit->second.name.empty())
            nb_name = nit->second.name.c_str();

        std::vector<std::size_t> same;
        for (std::size_t j = 0; j < out.size(); ++j)
            if (out[j].f.neighbour == v.f.neighbour)
                same.push_back(j);

        std::string tag;
        if (same.size() == 2)
        {
            const std::size_t other = (same[0] == i) ? same[1] : same[0];
            float c0, r0, c1, r1;
            span_centre(w, v.f, c0, r0);
            span_centre(w, out[other].f, c1, r1);
            if (std::fabs(r0 - r1) >= std::fabs(c0 - c1))
                tag = (r0 < r1 || (r0 == r1 && i < other)) ? "north" : "south";
            else
                tag = (c0 < c1 || (c0 == c1 && i < other)) ? "west" : "east";
        }
        else if (same.size() > 2)
        {
            const auto pos = std::find(same.begin(), same.end(), i) - same.begin();
            char buf[24];
            std::snprintf(buf, sizeof buf, "%d of %d", static_cast<int>(pos) + 1,
                          static_cast<int>(same.size()));
            tag = buf;
        }

        char buf[160];
        if (tag.empty())
            std::snprintf(buf, sizeof buf, "%s \xe2\x80\x94 %d tiles", nb_name,
                          static_cast<int>(v.f.span.size()));
        else
            std::snprintf(buf, sizeof buf, "%s \xe2\x80\x94 %d tiles (%s)", nb_name,
                          static_cast<int>(v.f.span.size()), tag.c_str());
        v.label = buf;
    }
}

} // namespace

const std::vector<front_view>& fronts_of(const world& w, entity_id nation)
{
    // Two slots: the canvas asks for the player's nation every frame, and a unit
    // card may ask for a rival's (an intel-only card still names its fronts).
    static fronts_cache slots[2];
    static int          next = 0;
    for (fronts_cache& c : slots)
        if (c.w == &w && c.nation == nation && c.day_tick == w.current_day_tick
            && c.territory == w.tile_to_nation.size())
            return c.views;

    fronts_cache& c = slots[next];
    next = (next + 1) % 2;
    c.w         = &w;
    c.nation    = nation;
    c.day_tick  = w.current_day_tick;
    c.territory = w.tile_to_nation.size();
    if (nation == null_entity)
        c.views.clear();
    else
        build_views(w, nation, c.views);
    return c.views;
}

const front_view* front_by_key(const world& w, entity_id nation, entity_id key)
{
    if (key == null_entity)
        return nullptr;
    for (const front_view& v : fronts_of(w, nation))
        if (v.f.key == key)
            return &v;
    return nullptr;
}

std::vector<entity_id> front_units(const world& w, entity_id nation, entity_id key)
{
    std::vector<entity_id> out;
    if (key == null_entity)
        return out;
    for (const auto& [uid, uc] : w.units)
        if (uc.owner == nation && uc.front == key && uc.count > 0)
            out.push_back(uid);
    std::sort(out.begin(), out.end());
    return out;
}

front_stance stance_toward(const world& w, entity_id nation, entity_id neighbour)
{
    if (is_hostile(w, nation, neighbour) || is_hostile(w, neighbour, nation))
        return front_stance::hostile;
    if (are_friends(w, nation, neighbour))
        return front_stance::friendly;
    return front_stance::neutral;
}

ImU32 front_stance_colour(front_stance s)
{
    switch (s)
    {
    case front_stance::hostile:  return palette::front_hostile;
    case front_stance::friendly: return palette::front_friendly;
    default:                     return palette::front_neutral;
    }
}

const char* front_stance_name(front_stance s)
{
    switch (s)
    {
    case front_stance::hostile:  return "Hostile";
    case front_stance::friendly: return "Friendly";
    default:                     return "Neutral";
    }
}

bool front_push_open(const world& w, entity_id nation, entity_id neighbour)
{
    return is_hostile(w, nation, neighbour);
}

} // namespace ui

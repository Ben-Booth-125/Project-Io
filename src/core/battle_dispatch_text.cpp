#include "battle_dispatch_text.hpp"

#include "world/world.hpp"

#include <array>
#include <cstdio>

namespace {

/// Stateless fold — the same idiom province.cpp uses for its edge jitter and
/// campaign_battle.cpp for its stream seed. NO RNG STREAM IS CONSUMED, which is
/// what lets the dispatch layer be written freely without any risk of it moving
/// the simulation.
std::size_t pick(uint64_t seed, int salt, std::size_t n)
{
    if (n == 0)
        return 0;
    uint64_t h = seed ^ (static_cast<uint64_t>(salt) * 0x9E3779B97F4A7C15ull);
    h ^= h >> 30; h *= 0xBF58476D1CE4E5B9ull;
    h ^= h >> 27; h *= 0x94D049BB133111EBull;
    h ^= h >> 31;
    return static_cast<std::size_t>(h % n);
}

const char* corp_name(const world& w, entity_id c)
{
    const auto it = w.corporations.find(c);
    // A missing corp degrades to a noun rather than an empty string: a dispatch
    // with a hole in it reads as a bug, and this layer must never be the thing
    // that looks broken.
    return (it != w.corporations.end() && !it->second.name.empty())
               ? it->second.name.c_str()
               : "an unmarked company";
}

// The banks. Sci-fi / neutral voice throughout — the standing rule is that real
// history is a mechanism reference and never a name source, and that governs the
// words a battle is reported in as much as it governs a nation's name. Nothing
// here is Earth-flavoured and nothing borrows a real formation, rank or place.
//
// %A and %D are substituted with the attacker's and defender's names.

constexpr std::array<const char*, 4> k_opening = {
    "Contact in %P. %A's line has closed with %D.",
    "%A has made contact with %D across %P. Neither side has committed.",
    "Skirmishing reported in %P — %A probing, %D holding.",
    "%P: %A and %D are in contact. Too early to read it.",
};

constexpr std::array<const char*, 4> k_committed = {
    "%P: both companies are committed now. %A at %a%%, %D at %d%%.",
    "The action in %P has settled into a grind. %A %a%%, %D %d%%.",
    "%A and %D are fully engaged in %P — %a%% against %d%%.",
    "No disengagement in %P. %A holds %a%%, %D holds %d%%.",
};

constexpr std::array<const char*, 4> k_crisis = {
    "%P has turned. %A %a%%, %D %d%% — the line moved hard this hour.",
    "Sharp reverse in %P: %A now %a%%, %D %d%%.",
    "The action in %P broke open. %A %a%%, %D %d%%.",
    "%P: a heavy exchange. %A down to %a%%, %D to %d%%.",
};

constexpr std::array<const char*, 4> k_wavering = {
    "%P is going. %A %a%%, %D %d%% — someone is about to break.",
    "The line in %P is failing. %A %a%%, %D %d%%.",
    "%P: one of them cannot take another exchange. %A %a%%, %D %d%%.",
    "Near collapse in %P — %A %a%%, %D %d%%.",
};

constexpr std::array<const char*, 3> k_broke_off = {
    "%W has broken off in %P. The ground is %F's, and it was not free.",
    "%W disengaged from %P at cost. %F holds the field.",
    "%P: %W pulled out. %F is left standing on it.",
};

constexpr std::array<const char*, 3> k_concluded = {
    "%P is decided. %F holds the field after %r rounds.",
    "The action in %P is over — %F holds it. %r rounds.",
    "%P: %F holds the ground. %r rounds, and it cost them.",
};

/// Substitute the small token set. Deliberately a hand-rolled pass rather than a
/// format library: the banks are authored strings and the token set is fixed, so
/// a full formatter would be machinery around four replacements.
std::string fill(const char* tmpl, const world& w, const battle_dispatch& d)
{
    char prov[32];
    std::snprintf(prov, sizeof prov, "province %u", d.province);
    char ap[16]; std::snprintf(ap, sizeof ap, "%d", d.attacker_strength_permille / 10);
    char dp[16]; std::snprintf(dp, sizeof dp, "%d", d.defender_strength_permille / 10);
    char rr[16]; std::snprintf(rr, sizeof rr, "%d", d.rounds_fought);

    const char* who_left =
        (d.withdrew == withdrawing_side::attacker)   ? corp_name(w, d.attacker)
        : (d.withdrew == withdrawing_side::defender) ? corp_name(w, d.defender)
                                                     : "neither side";
    const char* field = (d.field_held_by != null_entity) ? corp_name(w, d.field_held_by)
                                                         : "nobody";

    std::string out;
    for (const char* p = tmpl; *p; ++p)
    {
        if (*p != '%') { out.push_back(*p); continue; }
        switch (*(p + 1))
        {
            case 'A': out += corp_name(w, d.attacker); ++p; break;
            case 'D': out += corp_name(w, d.defender); ++p; break;
            case 'P': out += prov;      ++p; break;
            case 'a': out += ap;        ++p; break;
            case 'd': out += dp;        ++p; break;
            case 'r': out += rr;        ++p; break;
            case 'W': out += who_left;  ++p; break;
            case 'F': out += field;     ++p; break;
            case '%': out.push_back('%'); ++p; break;
            default:  out.push_back('%'); break;
        }
    }
    return out;
}

} // namespace

const char* battle_phase_word(battle_phase p)
{
    switch (p)
    {
        case battle_phase::opening:   return "Skirmishing";
        case battle_phase::committed: return "Committed";
        case battle_phase::crisis:    return "Crisis";
        case battle_phase::wavering:  return "Wavering";
        case battle_phase::broke_off: return "Breaking off";
        case battle_phase::concluded: return "Decided";
    }
    return "\xe2\x80\x94";
}

std::string battle_dispatch_line(const world& w, const battle_dispatch& d)
{
    // The round index salts the fold, so consecutive dispatches from ONE fight
    // read differently while the same fight replays identically — which is the
    // whole point of folding a seed rather than drawing.
    const int salt = d.rounds_fought;

    switch (d.phase)
    {
        case battle_phase::opening:
            return fill(k_opening[pick(d.stream_seed, salt, k_opening.size())], w, d);
        case battle_phase::committed:
            return fill(k_committed[pick(d.stream_seed, salt, k_committed.size())], w, d);
        case battle_phase::crisis:
            return fill(k_crisis[pick(d.stream_seed, salt, k_crisis.size())], w, d);
        case battle_phase::wavering:
            return fill(k_wavering[pick(d.stream_seed, salt, k_wavering.size())], w, d);
        case battle_phase::broke_off:
            return fill(k_broke_off[pick(d.stream_seed, salt, k_broke_off.size())], w, d);
        case battle_phase::concluded:
            return fill(k_concluded[pick(d.stream_seed, salt, k_concluded.size())], w, d);
    }
    // Unreachable while the enum is exhaustive above; a line rather than a crash
    // if a phase is ever appended without a bank.
    return fill("%P: %A and %D are engaged.", w, d);
}

// ---------------------------------------------------------------------------
// BL-577 — the contract dispatch bank, `battle_dispatch_line`'s sibling
// ---------------------------------------------------------------------------
// Five kinds, one bank each, ALWAYS spoken by the CLIENT NATION (%N) —
// CHAT.md's "the only speakers ever posted here are nations". %C names the
// contractor corp where the event has one; %P and %F are the province and
// fee. Sci-fi/neutral voice, same standing rule as the battle banks above —
// nothing here is Earth-flavoured and nothing borrows a real formation, rank
// or place.

namespace {

/// BL-577's own lookup, mirroring `corp_name` exactly — the Public channel is
/// nation-voiced, so a contract dispatch's SPEAKER is always resolved this way.
const char* nation_name(const world& w, entity_id n)
{
    const auto it = w.nations.find(n);
    return (it != w.nations.end() && !it->second.name.empty())
               ? it->second.name.c_str()
               : "an unrecognised polity";
}

constexpr std::array<const char*, 3> k_offer_issued = {
    "We have opened contract terms for %P - %F cr to whoever can answer them.",
    "%N is seeking a company for a matter in %P. Terms: %F cr.",
    "A contract stands open on %P. We offer %F cr to see it settled.",
};

constexpr std::array<const char*, 3> k_accepted = {
    "We have engaged %C to answer our contract in %P.",
    "%C has taken up our terms for %P.",
    "Contract for %P now stands with %C.",
};

constexpr std::array<const char*, 3> k_completed = {
    "%C has completed its contract with us in %P. Payment in full.",
    "The matter in %P is settled - %C delivered, and is paid.",
    "We confirm %C's contract for %P complete, and closed in their favour.",
};

constexpr std::array<const char*, 3> k_failed = {
    "%C failed to deliver on our contract for %P. The deposit is forfeit.",
    "Our contract with %C over %P has lapsed unmet. Nothing further is owed.",
    "%C did not answer our terms for %P in time. The matter is closed against them.",
};

constexpr std::array<const char*, 3> k_abandoned = {
    "%C has withdrawn from our contract for %P before its term.",
    "%C has abandoned the matter of %P. We consider it closed.",
    "Our terms with %C over %P stand withdrawn, by their own hand.",
};

/// Substitute the contract dispatch's own token set (%N nation, %C corp, %P
/// province, %F fee) — the same hand-rolled idiom `fill` (above) uses for the
/// battle banks, kept as its own small pass rather than shared: the two token
/// sets do not overlap and a shared formatter would need to know both.
std::string fill_contract(const char* tmpl, const world& w, const contract_dispatch& d)
{
    char prov[32];
    std::snprintf(prov, sizeof prov, "province %u", d.province);
    char fee[24];
    std::snprintf(fee, sizeof fee, "%d", static_cast<int>(d.fee));

    std::string out;
    for (const char* p = tmpl; *p; ++p)
    {
        if (*p != '%') { out.push_back(*p); continue; }
        switch (*(p + 1))
        {
            case 'N': out += nation_name(w, d.client);   ++p; break;
            case 'C': out += corp_name(w, d.contractor);  ++p; break;
            case 'P': out += prov;                        ++p; break;
            case 'F': out += fee;                         ++p; break;
            case '%': out.push_back('%');                 ++p; break;
            default:  out.push_back('%'); break;
        }
    }
    return out;
}

} // namespace

std::string contract_dispatch_line(const world& w, const contract_dispatch& d)
{
    // Folds the record's own stable id with the event kind — NO RNG DRAW, the
    // same BL-290 tongue-bank idiom `battle_dispatch_line` uses, just salted
    // by `kind` (a battle salts by round index; a contract record has no such
    // per-event counter, so the KIND is what varies the fold between the
    // several lines one contract can eventually speak).
    const int salt = static_cast<int>(d.what);

    switch (d.what)
    {
        case contract_dispatch::kind::offer_issued:
            return fill_contract(k_offer_issued[pick(d.id, salt, k_offer_issued.size())], w, d);
        case contract_dispatch::kind::accepted:
            return fill_contract(k_accepted[pick(d.id, salt, k_accepted.size())], w, d);
        case contract_dispatch::kind::completed:
            return fill_contract(k_completed[pick(d.id, salt, k_completed.size())], w, d);
        case contract_dispatch::kind::failed:
            return fill_contract(k_failed[pick(d.id, salt, k_failed.size())], w, d);
        case contract_dispatch::kind::abandoned:
            return fill_contract(k_abandoned[pick(d.id, salt, k_abandoned.size())], w, d);
    }
    // Unreachable while the enum is exhaustive above; a line rather than a
    // crash if a kind is ever appended without a bank.
    return fill_contract("A contract concerning %P has moved.", w, d);
}

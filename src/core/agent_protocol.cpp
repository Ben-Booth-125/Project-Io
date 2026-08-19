#include "agent_protocol.hpp"

#include "world/corp_ai.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

// The helpers and opcode handlers below moved VERBATIM from src/main.cpp's
// run_serve (BL-278) when BL-412 gave the protocol a second host. Their
// comments — the BL-396/BL-397 wire-validation history — moved with them,
// because the validation story is the part a future editor must not weaken.

namespace {

/// Parse the ` key=value` tokens of a request line into a lookup map.
/// The opcode itself (first token) is not included.
std::unordered_map<std::string, std::string> parse_kv_tokens(std::istringstream& iss)
{
    std::unordered_map<std::string, std::string> kv;
    std::string tok;
    while (iss >> tok)
    {
        const auto eq = tok.find('=');
        if (eq == std::string::npos)
            continue;
        kv[tok.substr(0, eq)] = tok.substr(eq + 1);
    }
    return kv;
}

long kv_get(const std::unordered_map<std::string, std::string>& kv, const char* key, long dflt)
{
    const auto it = kv.find(key);
    if (it == kv.end())
        return dflt;
    return std::atol(it->second.c_str());
}

/// BL-396: the checked sibling of kv_get, for the COMMAND parse — the one
/// opcode whose fields feed narrow types and array indices. Parses WIDE
/// (long long), then refuses (clears `ok`) anything that fails to parse as an
/// integer at all or falls outside [lo, hi], the destination's REAL domain.
/// Never truncates, never wraps, never clamps: `verb=256` used to truncate to
/// 0 and BUILD A BUILDING answering `applied`, `type=200` indexed the
/// per-type economics array out of bounds and segfaulted the process, and
/// `workforce=4294967396` wrapped to a legal 100 and applied. An absent key
/// keeps its default, which is in-domain by construction — absent is not
/// malformed, exactly kv_getf's reading.
///
/// strtoll saturates to LLONG_MAX/MIN on overflow, and every domain this
/// parser passes tops out at uint32's max, so the saturated value is itself
/// out of [lo, hi] and no errno check is needed.
long long kv_get_checked(const std::unordered_map<std::string, std::string>& kv,
                         const char* key, long long dflt,
                         long long lo, long long hi, bool* ok)
{
    const auto it = kv.find(key);
    if (it == kv.end())
        return dflt;
    char* end = nullptr;
    const long long v = std::strtoll(it->second.c_str(), &end, 10);
    // *end must be the terminator: "7xyz" is a malformed token, not a 7 with
    // decoration — tolerating the tail is the same silent substitution the
    // range check refuses, one lexeme later.
    if (end == it->second.c_str() || *end != '\0' || v < lo || v > hi)
    {
        *ok = false;
        return dflt;
    }
    return v;
}

/// Fractional sibling of kv_get. `quantity` and `floor_price` (BL-293) are real
/// quantities and real prices — routed through the integer getter above they
/// would silently truncate, so `floor_price=3.75` would list at 3 and undercut
/// the floor the caller asked for. A rounding error in a price is not a rounding
/// error, it is a different order.
///
/// NON-FINITE INPUT IS REJECTED, and `ok` is how — not by substituting the
/// default, which is a different order rather than a refused one.
///
/// `std::atof` happily parses "nan", "inf" and an overflowing "1e400", and the
/// seam's own validation cannot catch them: `place_sell_order` guards
/// `floor_price < 0.0f`, and every comparison against NaN is false, so a NaN
/// price passes validation, is stored in `world.sell_orders`, is folded into
/// `world::state_hash`, is written to the save stream — and reaches
/// `clear_markets`' book sort, where a value neither less than, equal to, nor
/// greater than any other stops the comparator being a strict weak ordering and
/// makes `std::sort` undefined behaviour. An infinity is the same story one step
/// later, overflowing a `static_cast<int>` in the procurement lead time.
///
/// The first cut of this guard returned the default instead, and that was wrong
/// for a reason worth keeping: the two keys sharing this getter have defaults
/// that mean OPPOSITE things downstream. `quantity`'s 0 is rejected by the seam,
/// so substituting it is a refusal by accident. `floor_price`'s 0 is meaningful
/// — the seam reads it as "accept the market price" — so substituting it turns
/// "sell only above this floor" into "sell at market, every tick", answers
/// `applied`, and issues no diagnostic. That is precisely the silent
/// order-substitution the truncation note above argues against, arrived at by a
/// different route.
double kv_getf(const std::unordered_map<std::string, std::string>& kv, const char* key,
               double dflt, bool* ok = nullptr)
{
    const auto it = kv.find(key);
    if (it == kv.end())
        return dflt; // absent is not malformed — the default stands
    // strtod with the same full-token rule as the integer path: atof parses
    // "abc" to 0.0, and 0.0 here means "accept the market price" — the exact
    // silent order-substitution the note above argues against.
    char* end = nullptr;
    const double v = std::strtod(it->second.c_str(), &end);
    if (end == it->second.c_str() || *end != '\0' || !std::isfinite(v))
    {
        if (ok) *ok = false;
        return dflt;
    }
    return v;
}

} // namespace

namespace agent_protocol {

const char* result_name(corp_command_result r)
{
    switch (r)
    {
        case corp_command_result::applied:          return "applied";
        case corp_command_result::rejected_no_corp:  return "rejected_no_corp";
        case corp_command_result::rejected_not_owner: return "rejected_not_owner";
        case corp_command_result::rejected_invalid:  return "rejected_invalid";
        case corp_command_result::rejected_placement: return "rejected_placement";
        case corp_command_result::rejected_funds:    return "rejected_funds";
        case corp_command_result::rejected_state:    return "rejected_state";
        case corp_command_result::rejected_tech_locked: return "rejected_tech_locked";
        case corp_command_result::rejected_era_locked: return "rejected_era_locked";
        case corp_command_result::rejected_depth_locked: return "rejected_depth_locked";
        // BL-350's four distinguishable declines. Without these the switch fell
        // through and reported every one of them as "rejected_invalid", which
        // tells an agent its arguments were malformed when in fact they were
        // fine and the SUPPLIER said no. Typed, enumerated failure is the whole
        // reason an out-of-process policy can correct itself (AI_OPPONENT.md
        // § 10a) — collapsing four business outcomes into a syntax error is the
        // one thing that seam must not do.
        //
        // Exhaustive now — but note that -Wswitch was ALREADY enabled and had
        // already been warning about exactly this. The build runs -Wall without
        // -Werror, so the warning was emitted on every compile and ignored. The
        // guard is the reader, not the flag.
        case corp_command_result::rejected_no_capacity:     return "rejected_no_capacity";
        case corp_command_result::rejected_no_input_access: return "rejected_no_input_access";
        case corp_command_result::rejected_embargo:         return "rejected_embargo";
        case corp_command_result::rejected_reputation:      return "rejected_reputation";
        case corp_command_result::rejected_cooldown:        return "rejected_cooldown"; // BL-430
    }
    return "rejected_invalid";
}

host_op service_line(const std::string& line, world& w, const recipe_registry& reg,
                     int tick, const session& s, std::string& out,
                     command_echo* echo)
{
    std::istringstream iss(line);
    std::string op;
    iss >> op;

    if (op == "SHUTDOWN")
    {
        out += "BYE\n";
        return host_op::shutdown;
    }
    else if (op == "TICK")
    {
        // The host owns the clock: run_serve steps the world and prints
        // "OK tick=<n>"; the live seam releases the app's gated clock and
        // answers when the boundary completes. Nothing to emit here.
        return host_op::tick;
    }
    else if (op == "BLACKBOARD")
    {
        const auto kv = parse_kv_tokens(iss);
        const entity_id corp = static_cast<entity_id>(kv_get(kv, "corp", 0));
        // BL-397: reads are gated by the same session actor as writes.
        // The blackboard is a corp's PRIVATE view — cash, pools, recipes —
        // and this opcode used to export whichever corp the line named.
        // The refusal keeps the lines-then-END shape a streaming client
        // frames by, so ERR here is a line before END, not a terminator.
        if (!s.as_any && corp != s.actor)
        {
            out += "ERR result=rejected_not_owner\n";
            out += "END\n";
            return host_op::none;
        }
        const int bb_ticks = static_cast<int>(kv_get(kv, "ticks", tick));
        const corp_blackboard bb = export_corp_blackboard(w, corp, bb_ticks);
        std::ostringstream body;
        to_jsonl(bb, body);
        out += body.str();
        out += "END\n";
        return host_op::none;
    }
    else if (op == "COMMAND")
    {
        const auto kv = parse_kv_tokens(iss);

        if (echo)
            echo->was_command = true;

        // BL-396: every integer field parses WIDE and is range-checked
        // against its destination's REAL domain before any narrowing cast
        // (kv_get_checked). The narrow static_casts this replaces were the
        // wire's whole validation story, and each failure mode was a
        // different lie: truncation applied a command the caller never
        // sent, a wrap applied a legal-looking value, and an out-of-enum
        // `type` indexed past the economics table. A violation rejects the
        // COMMAND whole, before apply_corp_command.
        bool ok = true;
        constexpr long long id_max =
            static_cast<long long>(std::numeric_limits<entity_id>::max());
        const long long verb_v = kv_get_checked(kv, "verb", 0, 0, corp_verb_count - 1, &ok);
        const long long corp_v = kv_get_checked(kv, "corp", 0, 0, id_max, &ok);
        const long long subj_v = kv_get_checked(kv, "subject", 0, 0, id_max, &ok);
        const long long tile_v = kv_get_checked(kv, "tile", 0, 0, id_max, &ok);
        const long long type_v = kv_get_checked(kv, "type", 0, 0, building_type_count - 1, &ok);
        const long long tgt_v  = kv_get_checked(kv, "target", 0, 0,
                                                static_cast<long long>(resource_count) - 1, &ok);
        const long long rcp_v  = kv_get_checked(kv, "recipe", no_recipe, 0, 0xFFFF, &ok);
        const long long wf_v   = kv_get_checked(kv, "workforce", 100, 0, 200, &ok);
        const long long road_v = kv_get_checked(kv, "road_tier", 1, 1, 3, &ok);
        // unit_type only needs to FIT its uint16 destination:
        // apply_corp_command range-checks it against the live
        // unit_roster_table() itself, so the wire's only job is stopping a
        // wrap from re-aiming it at a different roster row.
        const long long unit_v  = kv_get_checked(kv, "unit_type", 0, 0, 0xFFFF, &ok);
        const long long order_v = kv_get_checked(
            kv, "order", 0, 0,
            static_cast<long long>(std::numeric_limits<uint32_t>::max()), &ok);
        const long long cp_v    = kv_get_checked(kv, "counterparty", 0, 0, id_max, &ok);

        // Floats narrow FIRST, then test finiteness: 1e300 is a finite
        // double and an infinite float, and the seam stores floats.
        // kv_getf's own double-level guard still catches nan/inf/1e400 —
        // and see its comment for why the default must never stand in for
        // a malformed value. Negative quantity / floor_price is refused
        // here too: the seam guards them for place_sell_order, but a
        // malformed line should not reach the seam whatever the verb.
        bool floats_ok = true;
        const float qty_v   = static_cast<float>(kv_getf(kv, "quantity", 0.0, &floats_ok));
        const float floor_v = static_cast<float>(kv_getf(kv, "floor_price", 0.0, &floats_ok));
        if (!std::isfinite(qty_v) || !std::isfinite(floor_v)
            || qty_v < 0.0f || floor_v < 0.0f)
            floats_ok = false;

        if (!ok || !floats_ok)
        {
            out += "RESULT result=rejected_invalid building=-1\n";
            return host_op::none;
        }

        corp_command cmd;
        cmd.tick         = tick;
        cmd.corp         = static_cast<entity_id>(corp_v);
        cmd.verb         = static_cast<corp_verb>(verb_v);
        cmd.subject      = static_cast<entity_id>(subj_v);
        cmd.tile         = static_cast<entity_id>(tile_v);
        cmd.type         = static_cast<building_type>(type_v);
        cmd.target       = static_cast<resource_type>(tgt_v);
        cmd.recipe       = static_cast<uint16_t>(rcp_v);
        cmd.workforce    = static_cast<int>(wf_v);
        cmd.road_tier    = static_cast<uint8_t>(road_v);
        cmd.unit_type    = static_cast<uint16_t>(unit_v);
        cmd.quantity     = qty_v;
        cmd.floor_price  = floor_v;
        cmd.order        = static_cast<uint32_t>(order_v);
        cmd.counterparty = static_cast<entity_id>(cp_v);

        // BL-387: refuse to act as a corp the session is not. Before this
        // gate the only validation of `corp` was that the corp EXISTS, so
        // any caller could command any rival by name — verified in play,
        // rival balances moved by tens of millions. The gate sits here and
        // not in apply_corp_command: the seam must stay permissive for the
        // in-process scorer, which legitimately commands every rival.
        if (!s.as_any && cmd.corp != s.actor)
        {
            if (echo)
            {
                echo->parse_ok = true;
                echo->cmd      = cmd;
                echo->result   = corp_command_result::rejected_not_owner;
            }
            out += "RESULT result=rejected_not_owner building=-1\n";
            return host_op::none;
        }

        entity_id out_building = null_entity;
        const corp_command_result result = apply_corp_command(w, reg, cmd, &out_building);
        if (echo)
        {
            echo->parse_ok = true;
            echo->cmd      = cmd;
            echo->result   = result;
        }
        out += "RESULT result=";
        out += result_name(result);
        out += " building=";
        out += std::to_string(result == corp_command_result::applied
                                  ? static_cast<long>(out_building)
                                  : -1L);
        out += "\n";
        return host_op::none;
    }
    else if (op == "BODIES")
    {
        // What a body is called and whether it is surveyed yet. The sibling of
        // CORPS, and needed for the same reason (NR-061): `survey`,
        // `place_sell_order` and `request_quote` all take a BODY id as their
        // subject, and the protocol had no direct way to enumerate them.
        //
        // Precisely: the blackboard is not silent about bodies — `pool:<res>`
        // facts are keyed by the corp's own (corp, body) pool, and
        // `body_activity` by a known body — so an agent could recover the ids
        // of bodies it already trades on or already sees. What it could NOT
        // do is name a body it has no pool and no activity on, which is every
        // body worth SURVEYING, and it could never recover a name or a survey
        // phase for any of them. The market facts do not help: they are keyed
        // by MARKET id, so a price is legible on a body the agent cannot
        // address.
        // Ascending id, not map order. `w.bodies` is an unordered_map, and
        // an agent's transcript is a replay artifact — two runs of the same
        // seed must produce the same bytes, or a trace corpus records the
        // allocator's mood alongside the world's. CORPS sorts for the same
        // reason.
        std::vector<entity_id> body_ids;
        body_ids.reserve(w.bodies.size());
        for (const auto& [id, b] : w.bodies) body_ids.push_back(id);
        std::sort(body_ids.begin(), body_ids.end());
        std::ostringstream rows;
        for (const entity_id id : body_ids)
        {
            const body_component& b = w.bodies.at(id);
            const char* phase = "hidden";
            switch (b.survey.phase)
            {
                case survey_phase::hidden:     phase = "hidden";     break;
                case survey_phase::in_transit: phase = "in_transit"; break;
                case survey_phase::scanning:   phase = "scanning";   break;
                case survey_phase::surveyed:   phase = "surveyed";   break;
            }
            // NOTE for whoever reads this output: on a `hidden` body
            // regions_total is 0, and that means NOT YET COMPUTED, not
            // "nothing to survey" — the count is derived from the grid at
            // dispatch (survey_system.cpp). Reading 0 as "empty" is exactly
            // the misreading a pull-based fact interface invites, so the
            // field is emitted with the phase beside it rather than alone.
            rows << "{\"id\":" << static_cast<long>(id)
                 << ",\"name\":\"" << b.name << "\""
                 << ",\"survey\":\"" << phase << "\""
                 << ",\"regions_done\":" << b.survey.regions_done
                 << ",\"regions_total\":" << b.survey.regions_total
                 << "}" << '\n';
        }
        out += rows.str();
        out += "END\n";
        return host_op::none;
    }
    else if (op == "CORPS")
    {
        // Who can act on this seam: one JSON line per corporation, then END.
        // An agent's first question is "who am I?" — nothing else on the
        // protocol answers it (NR-061).
        // Ascending id, for the same reason BODIES sorts: a play transcript
        // is a replay artifact, and two runs of the same seed must agree
        // byte-for-byte or a trace corpus records the allocator's mood.
        std::vector<entity_id> corp_ids;
        corp_ids.reserve(w.corporations.size());
        for (const auto& [id, corp] : w.corporations) corp_ids.push_back(id);
        std::sort(corp_ids.begin(), corp_ids.end());
        std::ostringstream rows;
        for (const entity_id id : corp_ids)
        {
            const corporation_component& corp = w.corporations.at(id);
            rows << "{\"id\":" << static_cast<long>(id)
                 << ",\"name\":\"" << corp.name << "\""
                 << ",\"is_player\":" << (corp.is_player ? "true" : "false")
                 << ",\"home_nation\":" << static_cast<long>(corp.home_nation)
                 << "}" << '\n';
        }
        out += rows.str();
        out += "END\n";
        return host_op::none;
    }

    out += "ERR unknown op '" + op + "'\n";
    return host_op::none;
}

} // namespace agent_protocol

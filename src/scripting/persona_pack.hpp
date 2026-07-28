#pragma once

#include <memory>
#include <string>
#include <vector>

struct corp_blackboard;

// ---------------------------------------------------------------------------
// Persona counsel packs — the runtime for BL-207 (persona-runtime.md, Pantheon
// import). A pack is moddable Lua data (scripts/personas/*.lua) validated
// against pack_schema.lua; this module loads it into a SANDBOXED sol2
// environment (no io/os/package/require — mirrors lua_state's library set,
// never exposes a world pointer, only the flat blackboard-fact table) and
// runs its pure pipeline: blackboard -> extract -> findings -> policy ->
// opinion records, deterministically, within the pack's fixed per-eval
// budget. See scripts/personas/README.md for the pack contract.
// ---------------------------------------------------------------------------

namespace persona {

/// One opinion record, the Pantheon codebook grammar {p, m, on, c, w, v?}.
struct opinion_record
{
    std::string p;      ///< Authoring persona id.
    std::string m;       ///< Three-letter move code (e.g. "DTR").
    std::string on;     ///< Subject the opinion is about (e.g. "body#5").
    std::string c;       ///< In-voice claim, <= 140 chars.
    int         w  = 0;   ///< Weight, 0-3.
    std::string v;       ///< Verdict tag (SUFF/INSF/AMMIT/NOTYET/UNWG); empty if none.
};

/// One resolved disagreement, emitted only by a verdict-bench pack's aggregate().
struct verdict_record
{
    std::string v;         ///< SUFF/INSF/AMMIT/NOTYET/UNWG.
    std::string on;        ///< Subject the verdict resolves.
    std::string baseline;  ///< Named baseline the verdict is measured against.
    std::string c;         ///< In-voice explanation, <= 140 chars.
};

/// A loaded, schema-validated persona pack. Immutable once constructed; every
/// call is a pure function of its inputs (no world access, no clock, no RNG).
class pack
{
public:
    /// Loads and validates `lua_path` (via pack_schema.lua). Throws
    /// std::runtime_error on a Lua error or a schema violation.
    explicit pack(const std::string& lua_path);
    ~pack();
    pack(pack&&) noexcept;
    pack& operator=(pack&&) noexcept;
    pack(const pack&)            = delete;
    pack& operator=(const pack&) = delete;

    const std::string& id() const   { return m_id; }
    const std::string& bench() const { return m_bench; }
    const std::string& seal() const  { return m_seal; }
    const std::string& failure_condition() const { return m_failure_condition; }
    int                 budget() const { return m_budget; }
    bool                is_verdict_bench() const { return m_bench == "verdict"; }

    /// Runs extract(facts) then policy(findings), both bounded by budget().
    /// Facts beyond `budget()` are never marshalled into Lua at all (the fixed
    /// compute quota is enforced on the C++ side, not trusted to the pack).
    std::vector<opinion_record> evaluate(const corp_blackboard& bb) const;

    /// One deterministic in-voice line for an opinion record (calls phrase()).
    std::string phrase_for(const opinion_record& op) const;

    /// Verdict-bench only: resolves inter-persona disagreement in `opinions`
    /// (typically gathered from several packs' evaluate() over the same
    /// blackboard) into explicit verdict records. Throws if !is_verdict_bench().
    std::vector<verdict_record> aggregate(const std::vector<opinion_record>& opinions) const;

private:
    struct impl;
    std::unique_ptr<impl> m_impl; // hides sol::state from every non-Lua translation unit

    std::string m_id, m_bench, m_seal, m_failure_condition;
    int         m_budget = 0;
};

/// Loads every `*.lua` pack under `dir` (default scripts/personas), skipping
/// pack_schema.lua and README.md, sorted by filename for determinism. Throws
/// if any file fails schema validation.
std::vector<pack> load_bench(const std::string& dir = "scripts/personas");

} // namespace persona

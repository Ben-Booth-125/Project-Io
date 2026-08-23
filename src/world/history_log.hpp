#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// World history log — the append-only narrative substrate (BL-208, slice S1)
//
// One growing record of what happened, spanning deep time through the current
// tick. It replaces three scattered stores: planetology's per-body dated
// biography, continents' own parallel vector, and (later) the corp decision
// ring. Everything that used to be a one-shot report string becomes an entry.
//
// THE TWO STRUCTURAL FACTS THIS FILE IS SHAPED BY:
//
//  1. The genesis chapter and the campaign chapter have different LIFETIMES.
//     Genesis is a pure function of the campaign seed and is SEALED at tick 0.
//     Campaign entries record player and AI action and are unreproducible.
//     One entry type, two lifetimes — hence `seal()` rather than two classes.
//
//  2. This log is PERSISTED, even though genesis is theoretically derivable.
//     GENERATION_LEDGER.md's "regenerate on demand, do not persist" rule was
//     priced against six cheap tile passes. Against the accepted 30s-per-stage
//     generation ceiling you cannot re-run the chain to open a ledger. The split
//     is cost x volume, not principle: narrative entries (~10^2 per body,
//     seconds to regenerate) persist; the per-tile field breadcrumb (~10^4 per
//     body, milliseconds) keeps regenerating and does NOT belong here.
//
// THE SUMMARY/DEEP CONTRACT. A body's biography is capped at eight lines and
// that cap is load-bearing — PLANETOLOGY.md's brevity-is-characterisation rule
// means a Cradle gets eight lines and a Dead Rock gets three, and the asymmetry
// IS the characterisation. So the eight lines are `tier::headline` entries
// authored BY the gates, and everything else hangs beneath them as
// `tier::detail` by `parent`. `headlines()` is the cheap read; `children()` is
// the drill-down. The headline is chosen at emit time, deliberately: a gate
// knows which of its outcomes mattered, and selecting it afterward by ranking
// would turn a deliberate act into a heuristic's output.
//
// DELIBERATELY STANDALONE, exactly as chemistry_tables.hpp is. This header
// depends on the standard library only — not on components.hpp, planetology.hpp
// or world.hpp — so it can land and be tested ahead of any wiring.
//
// DETERMINISM. Appending consumes NO randomness and performs no floating-point
// comparison. The ordering key is an int64, which is a deliberate improvement
// over `history_event::gya`'s float: under this design the key becomes
// load-bearing, and a float comparison in a load-bearing path is exactly what
// the standing rules forbid. The per-subject index is an ordered std::map so
// iteration order never depends on hashing.
//
// Design authority: docs/development/backlog.json § BL-208 (designed
// 2026-07-29). Propagates into docs/ai/AI_OPPONENT.md, PLANETOLOGY.md,
// NATION_GENERATION.md and GENERATION_LEDGER.md when the work lands, per the
// standing authority-time-slice rule.
// ---------------------------------------------------------------------------

namespace hist {

/// Which chapter an entry belongs to. Eras are ORDERED, which is what lets
/// `(era, time_key)` totally order 4.5 Gyr and a single tick without forcing a
/// lossy shared unit on either end.
///
/// `deep_time` and `annals` are both SEALED (seed-derived, written once);
/// `campaign` is the growing segment. The lifetime boundary is therefore not the
/// era boundary — see `seal()`.
enum class era : uint8_t
{
    deep_time = 0, ///< S0-S9. The planetology chain. Dated in years before present.
    annals,        ///< Recorded history, once a toolmaker exists. Years before present.
    campaign,      ///< Play. Dated in ticks.
    count,
};

/// Summary or drill-down. See the summary/deep contract above.
enum class tier : uint8_t
{
    headline = 0, ///< One of the (at most eight) biography lines. Authored by the gate.
    detail,       ///< Hangs beneath a headline by `parent`. Unbounded.
    count,
};

enum class subject_kind : uint8_t
{
    body = 0,
    nation,
    region,
    corp,
    count,
};

/// Where a fact came from, so a reader can weight by source. Deliberately the
/// SAME vocabulary as the BL-206 blackboard export: there must be exactly one
/// visibility path in the codebase, because any divergence between two of them
/// is by definition a fog leak.
enum class provenance : uint8_t
{
    own_asset = 0,
    public_market,
    rival_visible,
    survey,
    route,
    count,
};

/// Which subject an entry is about. Packed for indexing; compared by the packed
/// form so ordering never depends on struct layout.
struct subject_ref
{
    subject_kind kind = subject_kind::body;
    uint32_t     id   = 0;

    uint64_t packed() const
    {
        return (static_cast<uint64_t>(kind) << 32) | static_cast<uint64_t>(id);
    }
};

inline bool operator==(const subject_ref& a, const subject_ref& b) { return a.packed() == b.packed(); }
inline bool operator!=(const subject_ref& a, const subject_ref& b) { return !(a == b); }

/// One machine-readable fact behind an entry's prose — which gate fired, which
/// scalar it read, which value it granted.
///
/// This is what makes the log answer "why does this tile hold iron x2.8" by a
/// JOIN rather than by more prose. Integer or fixed-point ONLY: a float here
/// would put a portability hazard into persisted data.
///
/// `key` is a namespaced uint16: the HIGH byte is the emitting domain, the LOW
/// byte is the field within it. This is what keeps this header free of any
/// dependency on the domains that write to it — chemistry does not leak in here,
/// and this file does not need to know what 0x0101 means.
///
///   0x01xx — chemistry / the molecular trace (BL-209). An 8-byte
///            molecular_event decomposes into four pairs: process, outcome,
///            yield_q, venue.
///   0x02xx — planetology scalars (thresholds read, margins, endowment grants).
///   0x03xx — annals / settlement history (BL-208 slice S4).
///   0x04xx — corp commands and score rationale (BL-202).
///
/// Register a new domain in this comment when you claim one.
struct citation
{
    uint16_t key   = 0;
    int32_t  value = 0;
};

/// Entry handle. Monotonic from 1; 0 means "none" and is a valid `parent` for a
/// headline. Assigned at append, so the tree is stable across runs.
using entry_id = uint32_t;

constexpr entry_id no_entry = 0;

/// One dated line, plus its causal payload.
///
/// `event` and `consequence` are the two prose columns carried over verbatim
/// from `history_event` — PLANETOLOGY.md § Presentation: the right column is the
/// entire feature, because "640 Myr of ferruginous ocean -> iron x2.8" is a
/// sentence a player argues with and a bare scalar is one they scroll past.
struct entry
{
    era        chapter = era::deep_time;
    tier       rank    = tier::headline;
    subject_ref subject{};

    /// Which gate or stage emitted this. Widened from `chain_stage` so the
    /// annals and campaign stages can share the field without this header
    /// depending on planetology.hpp.
    uint8_t stage = 0;

    /// Era-relative ordering key. For `deep_time` and `annals`, YEARS BEFORE
    /// PRESENT (so it counts DOWN toward the campaign). For `campaign`, the
    /// tick. Never a float — see the determinism note above.
    int64_t time_key = 0;

    entry_id   rank_parent = no_entry; ///< The headline this detail hangs under.
    provenance prov        = provenance::survey;

    std::string event;       ///< Left column — what happened.
    std::string consequence; ///< Right column — what it left behind. May be empty.

    std::vector<citation> citations;
};

/// The append-only log itself.
///
/// One reader, one writer, one per-subject index. Per-body and per-corp views
/// are index FILTERS over this single store, never separate stores — which is
/// what keeps "a nation collapsed during generation" and "a corp collapsed in
/// play" the same kind of fact.
class log
{
public:
    /// Append and return the assigned id. Consumes no randomness.
    ///
    /// A `detail` whose `rank_parent` does not resolve to an existing entry is a
    /// programming error; in release it is still appended (the log never throws
    /// away history) but `integrity_ok()` will report it.
    entry_id append(entry e);

    /// Convenience for the common case: a headline for one subject.
    entry_id headline(era chapter, subject_ref subj, uint8_t stage, int64_t time_key,
                      std::string ev, std::string cons = {},
                      provenance prov = provenance::survey);

    /// Convenience for the common case: a detail under a headline.
    entry_id detail(entry_id parent, uint8_t stage, int64_t time_key,
                    std::string ev, std::string cons = {},
                    std::vector<citation> cites = {});

    const entry& at(entry_id id) const;
    bool         valid(entry_id id) const { return id != no_entry && id <= entries_.size(); }
    std::size_t  size() const { return entries_.size(); }
    bool         empty() const { return entries_.empty(); }

    /// A subject's headline entries in APPEND order. Cheap (a reference), and the
    /// right choice when you want the order things were recorded in.
    ///
    /// NOT padded and NOT truncated: a world whose chain died at S2 returns three
    /// headlines, and that brevity is the characterisation (PLANETOLOGY.md
    /// § Presentation) — never pad it up to a fixed count.
    const std::vector<entry_id>& headlines(subject_ref subj) const;

    /// The summary read for DISPLAY: the same headlines ordered by
    /// (era, time_key), append index breaking ties.
    ///
    /// Use this for any reader that shows a biography. Append order is not
    /// chronological once a subject has more than one producer — a body's lines
    /// come from both the planetology chain and the continents pass.
    std::vector<entry_id> headlines_chronological(subject_ref subj) const;

    /// The deep read. One headline's detail entries, in emit order.
    std::vector<entry_id> children(entry_id parent) const;

    /// Seal the genesis chapter. Called once, at tick 0, after every generation
    /// pass has run. Entries at or below the seal are immutable history; the
    /// campaign segment grows after it.
    void seal();
    bool        sealed() const { return sealed_; }
    std::size_t sealed_count() const { return sealed_count_; }
    bool        is_sealed(entry_id id) const { return id != no_entry && id <= sealed_count_; }

    /// Self-check for the harness: ids are monotonic and dense from 1, every
    /// non-zero `rank_parent` resolves to an existing HEADLINE, no headline
    /// carries a parent, and nothing was appended to the sealed segment after
    /// `seal()`.
    bool integrity_ok() const;

private:
    std::vector<entry> entries_; ///< id == index + 1.

    /// Per-subject headline index. An ORDERED map so iteration never depends on
    /// hashing — the same reason planetology walks a sorted body list.
    std::map<uint64_t, std::vector<entry_id>> by_subject_;

    bool        sealed_       = false;
    std::size_t sealed_count_ = 0;

    /// Set when an append violated the tree or the seal. Reported by
    /// `integrity_ok()` rather than thrown: losing history is worse than
    /// recording a malformed entry, and the harness is where this must fail.
    bool breached_ = false;
};

} // namespace hist

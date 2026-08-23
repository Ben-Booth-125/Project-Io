#include "history_log.hpp"

#include <algorithm>

// ---------------------------------------------------------------------------
// World history log — implementation (BL-208, slice S1)
//
// Nothing here allocates randomness, compares floats, or iterates an unordered
// container. That is the whole contract; see history_log.hpp.
// ---------------------------------------------------------------------------

namespace hist {

namespace {

/// Returned for an out-of-range `at()`. A log read on a bad id must not be
/// undefined behaviour in a codebase where the caller is often UI code walking a
/// stale index.
const entry& null_entry()
{
    static const entry blank{};
    return blank;
}

const std::vector<entry_id>& empty_ids()
{
    static const std::vector<entry_id> none{};
    return none;
}

} // namespace

entry_id log::append(entry e)
{
    // The seal is a lifetime boundary, not an era boundary: once genesis is
    // sealed, only campaign entries may follow.
    if (sealed_ && e.chapter != era::campaign)
        breached_ = true;

    if (e.rank == tier::headline)
    {
        // A headline is a root by definition. Silently dropping a stray parent
        // would hide the bug, so record it and normalise.
        if (e.rank_parent != no_entry)
        {
            breached_    = true;
            e.rank_parent = no_entry;
        }
    }
    else
    {
        // A detail must hang under a headline that already exists — the tree is
        // built in causal order, so a forward reference is always an error.
        if (!valid(e.rank_parent) || entries_[e.rank_parent - 1].rank != tier::headline)
            breached_ = true;
    }

    entries_.push_back(std::move(e));
    const entry_id id = static_cast<entry_id>(entries_.size());

    if (entries_.back().rank == tier::headline)
        by_subject_[entries_.back().subject.packed()].push_back(id);

    return id;
}

entry_id log::headline(era chapter, subject_ref subj, uint8_t stage, int64_t time_key,
                       std::string ev, std::string cons, provenance prov)
{
    entry e;
    e.chapter     = chapter;
    e.rank        = tier::headline;
    e.subject     = subj;
    e.stage       = stage;
    e.time_key    = time_key;
    e.rank_parent = no_entry;
    e.prov        = prov;
    e.event       = std::move(ev);
    e.consequence = std::move(cons);
    return append(std::move(e));
}

entry_id log::detail(entry_id parent, uint8_t stage, int64_t time_key,
                     std::string ev, std::string cons, std::vector<citation> cites)
{
    entry e;
    // A detail inherits its chapter, subject and provenance from its headline —
    // it is the same fact at finer grain, and duplicating those at every call
    // site is how they drift apart.
    if (valid(parent))
    {
        const entry& h = entries_[parent - 1];
        e.chapter = h.chapter;
        e.subject = h.subject;
        e.prov    = h.prov;
    }
    e.rank        = tier::detail;
    e.stage       = stage;
    e.time_key    = time_key;
    e.rank_parent = parent;
    e.event       = std::move(ev);
    e.consequence = std::move(cons);
    e.citations   = std::move(cites);
    return append(std::move(e));
}

const entry& log::at(entry_id id) const
{
    if (!valid(id))
        return null_entry();
    return entries_[id - 1];
}

const std::vector<entry_id>& log::headlines(subject_ref subj) const
{
    const auto it = by_subject_.find(subj.packed());
    return it == by_subject_.end() ? empty_ids() : it->second;
}

std::vector<entry_id> log::headlines_chronological(subject_ref subj) const
{
    std::vector<entry_id> out = headlines(subj);

    // Ordered by (era, time_key) — the total order BL-208's design names — with
    // APPEND INDEX as the tie-break so the result is fully determined.
    //
    // This cannot be the raw append order: a subject has more than one producer
    // (the planetology chain and the continents pass both write a body's
    // headlines), and two producers cannot append in chronological order between
    // them. `deep_time` and `annals` count DOWN in years before present, so a
    // LARGER time_key is EARLIER; `campaign` counts up in ticks. Sorting by era
    // first keeps both conventions correct without a shared unit.
    std::stable_sort(out.begin(), out.end(), [this](entry_id a, entry_id b) {
        const entry& x = entries_[a - 1];
        const entry& y = entries_[b - 1];
        if (x.chapter != y.chapter)
            return static_cast<uint8_t>(x.chapter) < static_cast<uint8_t>(y.chapter);
        if (x.time_key != y.time_key)
            return x.chapter == era::campaign ? x.time_key < y.time_key
                                              : x.time_key > y.time_key;
        return a < b;
    });
    return out;
}

std::vector<entry_id> log::children(entry_id parent) const
{
    std::vector<entry_id> out;
    if (!valid(parent))
        return out;

    // Linear over the store rather than a second index. Details are emitted
    // immediately after their headline, the counts are ~10^2 per body, and one
    // fewer index is one fewer thing that can fall out of sync.
    for (std::size_t i = 0; i < entries_.size(); ++i)
    {
        if (entries_[i].rank == tier::detail && entries_[i].rank_parent == parent)
            out.push_back(static_cast<entry_id>(i + 1));
    }
    return out;
}

void log::seal()
{
    sealed_       = true;
    sealed_count_ = entries_.size();
}

bool log::integrity_ok() const
{
    if (breached_)
        return false;

    for (std::size_t i = 0; i < entries_.size(); ++i)
    {
        const entry& e = entries_[i];

        if (e.rank == tier::headline)
        {
            if (e.rank_parent != no_entry)
                return false;
            continue;
        }

        // Parent must exist, be a headline, and precede this entry.
        if (e.rank_parent == no_entry || e.rank_parent > i)
            return false;
        if (entries_[e.rank_parent - 1].rank != tier::headline)
            return false;
    }

    // Every indexed headline id must resolve to a headline of that subject.
    for (const auto& [packed, ids] : by_subject_)
    {
        for (const entry_id id : ids)
        {
            if (!valid(id))
                return false;
            const entry& e = entries_[id - 1];
            if (e.rank != tier::headline || e.subject.packed() != packed)
                return false;
        }
    }

    return true;
}

} // namespace hist

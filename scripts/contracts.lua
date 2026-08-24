-- Project Io — contracts.lua
-- The mercenary-contract template table (BL-570; docs/economy/CONTRACTS.md
-- § The mercenary contract). "The variety of contract types is authored data
-- in the predicate, not a type enum in C++ ... adding a contract kind is a
-- Lua change." Loaded at startup into src/world/contract_template.{hpp,cpp}'s
-- registry (contract_template_registry::load_from_lua).
--
-- Each row is:
--   { id, name, predicate = { subject, comparator, operand },
--     continuous, fee_mult, deadline_ticks }
--
--   id             — stable slug a saved contract's template reference
--                    resolves back to (contract_template_registry::index_of).
--                    Reordering rows is safe; renaming or removing an id that
--                    a save already references is not.
--   name           — human-facing label.
--   predicate      — the single fact the contract pays to make true. `subject`
--                    names a condition_subject (src/world/condition_set.hpp);
--                    `comparator` names a condition_comparator. There is no
--                    `province` field here — it is NOT authored: the province
--                    a contract targets is bound per accepted offer (BL-572
--                    derives it, BL-573 binds it into the live contract), so
--                    the same row serves every province a client ever offers.
--   continuous     — false: the predicate need only hold AT the deadline
--                    ("take" — losing and retaking the province before then
--                    still completes it). true: it must hold on EVERY tick up
--                    to the deadline, or the contract fails early ("hold").
--   fee_mult       — multiplier the offer-derivation pass (BL-572,
--                    derive_contract_offers) applies to the base fee this
--                    kind of work commands.
--   deadline_ticks — ticks from acceptance to the deadline.
--
-- Magnitudes below are legible round defaults, not derived (matching
-- economy.lua's own discipline) — nothing reads fee_mult or deadline_ticks
-- yet (that is BL-572/BL-573, Wave 3/4), so there is nothing to measure
-- against. Iterate by playtest once a contract can actually be accepted.

contract_templates = {
    -- id 0 — take: seize a province the client does not currently hold. A
    -- point-in-time predicate — only the state AT the deadline is paid for,
    -- so losing and retaking the province before then still completes it.
    {
        id   = "take",
        name = "Take the Province",
        predicate = {
            subject    = "province_held",
            comparator = "at_least",
            operand    = 1,
        },
        continuous     = false,
        fee_mult       = 1.0,
        deadline_ticks = 180,
    },

    -- id 1 — hold: defend a province the client already holds. A continuous
    -- predicate — the client is paying for the province to STAY held, so a
    -- single tick of loss fails the contract even if it is later retaken.
    -- Priced above "take" (fee_mult) and given a shorter fuse
    -- (deadline_ticks): standing watch commands a premium, and a defensive
    -- window is naturally shorter than an offensive campaign's.
    {
        id   = "hold",
        name = "Hold the Province",
        predicate = {
            subject    = "province_held",
            comparator = "at_least",
            operand    = 1,
        },
        continuous     = true,
        fee_mult       = 1.25,
        deadline_ticks = 90,
    },
}

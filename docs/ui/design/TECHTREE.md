# Tech Tree — Design Notes

Working notes on how the tech tree panel should look. Not an authority doc yet —
capture thoughts here as they come; promote settled decisions into the owning
UI doc once they land.

## Layout

- No central node. Every node starts on the first ring.
- Some first-ring nodes branch off into their own separate sub-trees, rather
  than everything converging back to one root.

## Colour / node state

Five colours, replacing the current "undefined" greyed segments (those won't
exist in the final product — grey is repurposed as a real state below):

| Colour | Meaning |
|---|---|
| Blue | Milestone tech |
| Grey | Minor tech |
| Orange | In progress |
| Green | Complete (any type) |
| Red | Journal / quest tech (always red, regardless of milestone/minor) |

A node that isn't unlocked but already has tasks assigned to unlock it uses
the greyed-out treatment (i.e. grey now doubles as both "minor tech" and
"has assigned unlock tasks, not yet unlocked" — needs disambiguating, or the
two need distinct treatments — pick up tomorrow).

## Info box

Not a pure hover card. Opens on **click**, or on **hover held for 1 second**
— the hover path shows a progress wheel filling over that second before the
box appears.

## Style reference

Style name not yet settled. Reference render: `./renders/techtree/wide8`.
Pick this back up tomorrow — settle the name and confirm against the wide8
reference images.

Working genre definition (2026-08-26, see `GLOBAL_STYLE_SHEET.md` for full context):

> "Industrial schematic HMI" — flat-shaded, monochrome-plus-one-accent, hard-edged,
> diegetic machine terminal. Minimal by subtraction (no glow, no gradient, no chrome)
> rather than by whitespace.

More references being gathered under `./renders/techtree/refs/` (on-style / off-style).

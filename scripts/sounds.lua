-- Project Io — the cue table (docs/ui/SOUND.md).
--
-- Every sound in the game is synthesised from this table at start-up; there are no audio
-- files. tools/session/sound_cues.js reads this same file to build the listening page, so
-- keep it to plain literals: tables, strings, numbers, `--` comments. No expressions.
--
-- A cue is a list of notes mixed together. A note:
--   wave  "sine" | "tri" | "square" | "noise"
--   f     start pitch, Hz (ignored for noise)
--   f1    end pitch, Hz — an exponential glide across the note (default: f)
--   at    start offset inside the cue, seconds (default 0)
--   a     attack, seconds, linear rise
--   h     hold at full level, seconds (default 0)
--   r     release, seconds to -60 dB, exponential
--   g     gain, 0..1
--   lp    one-pole low-pass cutoff, Hz (default: none)
--   lp1   end cutoff, Hz — an exponential sweep across the note (needs lp; default: lp)
-- A cue also carries:
--   group     "ui" | "event"   — which volume slider it answers to
--   priority  1..9, higher survives the voice cap (SOUND.md § 6)
--   gain      the cue's overall level, 0..1
--   when      plain words: what fires it (read by the listening page, not the game)

return {
  cues = {
    -- UI presses ------------------------------------------------------------------------
    ui_press = { when = "Any button press in the shell or a ledger that no more specific cue claims.",
      group = "ui", priority = 2, gain = 0.32, notes = {
      { wave = "noise", a = 0.0005, r = 0.006, g = 0.45, lp = 7000 },
      { wave = "sine",  f = 2100, a = 0.0008, r = 0.014, g = 0.7 },
    }},
    ui_toggle_on = { when = "A visible-state toggle switches on: a lens/overlay, a panel flag.",
      group = "ui", priority = 2, gain = 0.32, notes = {
      { wave = "noise", a = 0.0005, r = 0.006, g = 0.4, lp = 7000 },
      { wave = "sine",  f = 1700, a = 0.0008, r = 0.012, g = 0.7 },
      { wave = "noise", at = 0.022, a = 0.0005, r = 0.006, g = 0.4, lp = 7000 },
      { wave = "sine",  f = 2300, at = 0.022, a = 0.0008, r = 0.016, g = 0.8 },
    }},
    ui_toggle_off = { when = "A visible-state toggle switches off.",
      group = "ui", priority = 2, gain = 0.32, notes = {
      { wave = "noise", a = 0.0005, r = 0.006, g = 0.4, lp = 7000 },
      { wave = "sine",  f = 2300, a = 0.0008, r = 0.012, g = 0.7 },
      { wave = "noise", at = 0.022, a = 0.0005, r = 0.006, g = 0.4, lp = 6000 },
      { wave = "sine",  f = 1700, at = 0.022, a = 0.0008, r = 0.016, g = 0.8 },
    }},
    ui_refused = { when = "A press the game refuses: a disabled button clicked, or a command the sim rejects (build, road, demolish, survey, any player order).",
      group = "ui", priority = 3, gain = 0.4, notes = {
      { wave = "square", f = 196, f1 = 175, a = 0.003, h = 0.03, r = 0.06, g = 0.6, lp = 700 },
    }},
    ui_ledger_open = { when = "A ledger opens: nav rail, alert click-through.",
      group = "ui", priority = 2, gain = 0.32, notes = {
      { wave = "noise", a = 0.0005, r = 0.008, g = 0.35, lp = 6000 },
      { wave = "sine",  f = 1400, a = 0.001, r = 0.02, g = 0.8 },
      { wave = "noise", at = 0.035, a = 0.0005, r = 0.008, g = 0.35, lp = 6000 },
      { wave = "sine",  f = 1900, at = 0.035, a = 0.001, r = 0.025, g = 0.9 },
    }},
    ui_ledger_close = { when = "A ledger closes: rail re-click, active-tab re-click, Esc.",
      group = "ui", priority = 2, gain = 0.32, notes = {
      { wave = "noise", a = 0.0005, r = 0.008, g = 0.35, lp = 6000 },
      { wave = "sine",  f = 1900, a = 0.001, r = 0.02, g = 0.8 },
      { wave = "noise", at = 0.035, a = 0.0005, r = 0.008, g = 0.35, lp = 5000 },
      { wave = "sine",  f = 1400, at = 0.035, a = 0.001, r = 0.025, g = 0.9 },
    }},
    ui_tab = { when = "Switching to a different tab inside an open ledger.",
      group = "ui", priority = 1, gain = 0.24, notes = {
      { wave = "noise", a = 0.0005, r = 0.005, g = 0.4, lp = 8000 },
      { wave = "sine",  f = 2600, a = 0.0008, r = 0.01, g = 0.6 },
    }},

    -- Alerts — one cue per severity tier, as an alert first appears -----------------------
    alert_low = { when = "A low-tier alert first appears: research not set.",
      group = "event", priority = 5, gain = 0.4, notes = {
      { wave = "sine", f = 880,  a = 0.005, h = 0.04, r = 0.18, g = 0.8 },
      { wave = "sine", f = 1175, at = 0.12, a = 0.005, h = 0.04, r = 0.25, g = 0.8 },
    }},
    alert_mid = { when = "A mid-tier alert first appears: idle buildings, construction stalled, deposit exhausted, trades exceed points.",
      group = "event", priority = 6, gain = 0.45, notes = {
      { wave = "tri", f = 784, a = 0.005, h = 0.05, r = 0.15, g = 0.9 },
      { wave = "tri", f = 622, at = 0.13, a = 0.005, h = 0.05, r = 0.2, g = 0.9 },
      { wave = "tri", f = 784, at = 0.42, a = 0.005, h = 0.05, r = 0.15, g = 0.7 },
      { wave = "tri", f = 622, at = 0.55, a = 0.005, h = 0.05, r = 0.25, g = 0.7 },
    }},
    alert_high = { when = "A high-tier alert first appears: at war, low runway.",
      group = "event", priority = 8, gain = 0.45, notes = {
      { wave = "sine",   f = 988, a = 0.003, h = 0.06, r = 0.12, g = 0.9 },
      { wave = "square", f = 494, a = 0.003, h = 0.06, r = 0.1,  g = 0.12, lp = 1500 },
      { wave = "sine",   f = 740, at = 0.14, a = 0.003, h = 0.06, r = 0.16, g = 0.9 },
      { wave = "square", f = 370, at = 0.14, a = 0.003, h = 0.06, r = 0.12, g = 0.12, lp = 1500 },
      { wave = "sine",   f = 988, at = 0.42, a = 0.003, h = 0.06, r = 0.12, g = 0.75 },
      { wave = "square", f = 494, at = 0.42, a = 0.003, h = 0.06, r = 0.1,  g = 0.1, lp = 1500 },
      { wave = "sine",   f = 740, at = 0.56, a = 0.003, h = 0.06, r = 0.22, g = 0.75 },
      { wave = "square", f = 370, at = 0.56, a = 0.003, h = 0.06, r = 0.16, g = 0.1, lp = 1500 },
    }},

    -- Construction -----------------------------------------------------------------------
    construction_complete = { when = "One of your buildings finishes construction, on the active canvas.",
      group = "event", priority = 4, gain = 0.4, notes = {
      { wave = "sine", f = 660, a = 0.004, h = 0.03, r = 0.2,  g = 0.8 },
      { wave = "sine", f = 990, at = 0.09, a = 0.004, h = 0.05, r = 0.35, g = 0.9 },
      { wave = "sine", f = 1980, at = 0.09, a = 0.004, r = 0.2, g = 0.15 },
    }},
    construction_stalled = { when = "One of your construction sites stalls, on the active canvas.",
      group = "event", priority = 5, gain = 0.4, notes = {
      { wave = "tri", f = 523, a = 0.004, h = 0.04, r = 0.15, g = 0.8, lp = 1800 },
      { wave = "tri", f = 392, at = 0.11, a = 0.004, h = 0.06, r = 0.3, g = 0.8, lp = 1400 },
    }},

    -- Conflict ---------------------------------------------------------------------------
    war_declared = { when = "A war is declared in which your nation is a party (either side). Global.",
      group = "event", priority = 9, gain = 0.45, notes = {
      { wave = "sine",   f = 1568, a = 0.002, r = 0.12, g = 0.08 },
      { wave = "tri",    f = 392, a = 0.004, h = 0.08, r = 0.22, g = 0.8 },
      { wave = "tri",    f = 466, a = 0.004, h = 0.08, r = 0.22, g = 0.6 },
      { wave = "square", f = 196, f1 = 185, a = 0.004, h = 0.08, r = 0.18, g = 0.2, lp = 1200 },
      { wave = "sine",   f = 98, a = 0.01, h = 0.08, r = 0.25, g = 0.45 },
      { wave = "tri",    f = 349, at = 0.22, a = 0.004, h = 0.1, r = 0.4, g = 0.85 },
      { wave = "tri",    f = 415, at = 0.22, a = 0.004, h = 0.1, r = 0.4, g = 0.65 },
      { wave = "square", f = 175, f1 = 165, at = 0.22, a = 0.004, h = 0.1, r = 0.3, g = 0.22, lp = 1000 },
      { wave = "sine",   f = 87, at = 0.22, a = 0.01, h = 0.1, r = 0.45, g = 0.5 },
    }},
    battle_begin = { when = "A battle begins involving your units, on the active canvas.",
      group = "event", priority = 7, gain = 0.5, notes = {
      { wave = "noise", a = 0.002, r = 0.15, g = 0.3, lp = 900 },
      { wave = "tri",   f = 147, f1 = 130, a = 0.003, h = 0.05, r = 0.2, g = 1.0 },
      { wave = "noise", at = 0.2, a = 0.002, r = 0.15, g = 0.3, lp = 900 },
      { wave = "tri",   f = 147, f1 = 130, at = 0.2, a = 0.003, h = 0.05, r = 0.3, g = 1.0 },
    }},

    -- Research ---------------------------------------------------------------------------
    research_complete = { when = "Your nation completes a technology. Global.",
      group = "event", priority = 5, gain = 0.4, notes = {
      { wave = "sine", f = 784,  a = 0.004, h = 0.03, r = 0.25, g = 0.8 },
      { wave = "sine", f = 988,  at = 0.09, a = 0.004, h = 0.03, r = 0.25, g = 0.8 },
      { wave = "sine", f = 1175, at = 0.18, a = 0.004, h = 0.06, r = 0.6, g = 0.9 },
      { wave = "sine", f = 2350, at = 0.18, a = 0.004, r = 0.3, g = 0.12 },
    }},

    -- Time -------------------------------------------------------------------------------
    time_pause = { when = "Time pauses (button, key or menu).",
      group = "ui", priority = 3, gain = 0.34, notes = {
      { wave = "noise", a = 0.0005, r = 0.006, g = 0.4, lp = 5000 },
      { wave = "sine",  f = 1300, a = 0.0008, r = 0.018, g = 0.75 },
      { wave = "noise", at = 0.03, a = 0.0005, r = 0.006, g = 0.4, lp = 4000 },
      { wave = "sine",  f = 900, at = 0.03, a = 0.0008, r = 0.028, g = 0.75 },
    }},
    time_resume = { when = "Time resumes from pause.",
      group = "ui", priority = 3, gain = 0.34, notes = {
      { wave = "noise", a = 0.0005, r = 0.006, g = 0.4, lp = 4000 },
      { wave = "sine",  f = 900, a = 0.0008, r = 0.018, g = 0.75 },
      { wave = "noise", at = 0.03, a = 0.0005, r = 0.006, g = 0.4, lp = 5000 },
      { wave = "sine",  f = 1300, at = 0.03, a = 0.0008, r = 0.028, g = 0.75 },
    }},
    time_faster = { when = "Game speed goes up a step.",
      group = "ui", priority = 2, gain = 0.28, notes = {
      { wave = "noise", a = 0.0005, r = 0.006, g = 0.4, lp = 7000 },
      { wave = "sine",  f = 1800, a = 0.0008, r = 0.01, g = 0.75 },
      { wave = "noise", at = 0.018, a = 0.0005, r = 0.006, g = 0.4, lp = 7000 },
      { wave = "sine",  f = 2200, at = 0.018, a = 0.0008, r = 0.01, g = 0.75 },
      { wave = "noise", at = 0.036, a = 0.0005, r = 0.006, g = 0.4, lp = 7000 },
      { wave = "sine",  f = 2600, at = 0.036, a = 0.0008, r = 0.014, g = 0.75 },
    }},
    time_slower = { when = "Game speed goes down a step.",
      group = "ui", priority = 2, gain = 0.28, notes = {
      { wave = "noise", a = 0.0005, r = 0.006, g = 0.4, lp = 7000 },
      { wave = "sine",  f = 2600, a = 0.0008, r = 0.01, g = 0.75 },
      { wave = "noise", at = 0.018, a = 0.0005, r = 0.006, g = 0.4, lp = 7000 },
      { wave = "sine",  f = 2200, at = 0.018, a = 0.0008, r = 0.01, g = 0.75 },
      { wave = "noise", at = 0.036, a = 0.0005, r = 0.006, g = 0.4, lp = 7000 },
      { wave = "sine",  f = 1800, at = 0.036, a = 0.0008, r = 0.014, g = 0.75 },
    }},

    -- Space ------------------------------------------------------------------------------
    space_launch = { when = "You launch a space asset or unit, from a body on the active canvas.",
      group = "event", priority = 6, gain = 0.5, notes = {
      { wave = "noise", a = 0.12, h = 0.45, r = 0.35, g = 0.6, lp = 250, lp1 = 3200 },
      { wave = "noise", a = 0.2, h = 0.3, r = 0.3, g = 0.25, lp = 600, lp1 = 6000 },
    }},
    space_arrival = { when = "Your space asset or unit arrives in orbit or at a body on the active canvas.",
      group = "event", priority = 5, gain = 0.5, notes = {
      { wave = "noise", a = 0.06, h = 0.3, r = 0.45, g = 0.6, lp = 3200, lp1 = 250 },
      { wave = "noise", a = 0.04, h = 0.2, r = 0.3, g = 0.25, lp = 6000, lp1 = 600 },
    }},
  },

  -- The bed (SOUND.md § 7): slow partials whose level drifts, plus a breath of filtered noise.
  -- Muted (gain 0) pending a rework (Ben, 2026-11-24): the mixer skips a zero-gain bed.
  bed = {
    gain = 0,
    partials = {
      { f = 55,    g = 0.6, lfo = 0.031, depth = 0.5 },
      { f = 82.4,  g = 0.4, lfo = 0.047, depth = 0.6 },
      { f = 110.3, g = 0.3, lfo = 0.023, depth = 0.7 },
      { f = 164.8, g = 0.15, lfo = 0.061, depth = 0.8 },
    },
    noise = { g = 0.08, lp = 300 },
    duck = { depth = 0.5, a = 0.03, r = 0.6 },
  },
}

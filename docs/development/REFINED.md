# Project Io — REFINED (active worklist)

*Empty between work blocks.*

**Sprint 26 (watch the AI play) CLOSED 2026-08-31 AT WAVE 1 — goal met.** Spectator mode works
(`--spectate`), the god-view control was already there, the decision feed reports real scores and
reasons, a composite standing index exists, and the 1960s start is selectable. Five items landed,
one closed unbuilt, four build-wiring breaks fixed on the way.

Wave 2 was **cancelled rather than built**, on the sprint's own measurement: every AI corp is
insolvent 29–30 ticks of 30 on every seed, because on the industrial band `iron_ore` is produced
42991.6 against total demand **0.000**. A coalition brake forms against *whoever leads*, and among
corps that are all deeply insolvent the leader is merely the least bankrupt. NR-758 carries the
measurement; BL-697, BL-699, BL-703, BL-704 and BL-698 carry out unbuilt, blocked on demand.

---

**Sprint 27 (demand, resumed) is OPEN.** It resumes sprint 21's waves 1+ against that measurement.

**BL-654 (a channel must bid) is the first item and it gates the rest.** Two of the three live
demand channels are pool draws that never reach `market_component::demand`, so wanting a good never
induces its supply — which is why BL-641 shipped its upkeep rates at zero after turning them on
collapsed operating firms from 227 to 19. Ben settled the shape on 2026-08-26: *"Buy on the market,
but at a threshold, buying is not allowed."* One rule for every goods draw, unit upkeep included.
Turning BL-641's rates non-zero is gated on it landing.

The sprint's success condition is **not** "the channels are built" — it is that `ai_skill_harness`
shows a field that is not monotonically insolvent. Channels built and corps still broke is exactly
what BL-641 produced on its own.

**Open work with no promoted tasks:** `node tools/session/backlog_query.js --table`.

-- Verify the comms chat log (BL-205 R1) — the panel that replaced the Explorer.
-- The panel sits in the right shell band (below the time column, above the
-- minimap) with the Public channel tab, the deterministic epoch system line,
-- and any background-corp agency messages the run produced.
verify.econ_step(12)
verify.capture("chat_panel")

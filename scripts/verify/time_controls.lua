-- Verify the time-speed curve (BL-007 / time-speed-curve R1).
-- Captures the default Solar canvas view to show the time column with speed
-- buttons and the econ-tick quarter progress bar.
-- Code checks (R2: speed table, R3: ProgressBar empty overlay) are verified
-- separately by code inspection.
verify.capture("time_controls_default")

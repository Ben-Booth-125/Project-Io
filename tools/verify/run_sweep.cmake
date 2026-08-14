# run_sweep.cmake — the sweep harnesses' opt-in gate (BL-415).
#
# Invoked as each sweep-labelled test's COMMAND with -DIO_SWEEP_EXE=<exe>. A
# bare `ctest` reaches this script, prints the skip line, and CTest marks the
# test "Skipped" via its SKIP_REGULAR_EXPRESSION property — so exclusion from
# the routine gate is machinery, not a `-LE sweep` flag the caller has to
# remember (that label-only scheme protected exactly whoever typed it, and a
# forgotten flag once left two orphaned history_sim_harness processes burning
# ~5,200 CPU-seconds each).
#
# Deliberate runs opt in with the environment variable:
#   PowerShell:  $env:IO_RUN_SWEEPS=1; ctest --test-dir build -L sweep
#   bash:        IO_RUN_SWEEPS=1 ctest --test-dir build -L sweep
# or bypass CTest entirely and run the exe — a sweep is a research instrument
# and its printed output is the product.

if(NOT DEFINED ENV{IO_RUN_SWEEPS})
    message("sweep harness skipped (set IO_RUN_SWEEPS=1 to run)")
    return()
endif()

execute_process(COMMAND "${IO_SWEEP_EXE}" RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "sweep harness failed with exit code ${rc}")
endif()

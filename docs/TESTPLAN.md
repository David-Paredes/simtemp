**T1 — Load/Unload:** build → `insmod` → verify `/dev/simtemp` & sysfs → `rmmod` (no warnings).
- Check if path /dev/simtemp exists
- If the path exists the test is passed.

**T2 — Periodic Read:** set `sampling_ms=100`; verify ~10±1 samples/sec using timestamps.
- Write config sampling_mc = 100.
- Obtain the number of samples in 1 second with count = print_sample_test(2, ep, fd).
- If count is between 9 and 11 the test is passed.

**T3 — Threshold Event:** lower threshold slightly below mean; ensure `poll` unblocks within 2–3 periods and flag is set.
- Write config mode = NORMAL_MODE
- Write config theshold_mc = 24990 (below mean of 25000)
- Obtain the number of samples until threshold alert is reveived with count = print_sample_test(3, ep, fd)
- If count is lower or equal than 3, the test is passed.

**T4 — Error Paths:** invalid sysfs writes → `-EINVAL`; very fast sampling (e.g., `1ms`) doesn’t wedge; `stats` still increments.
- Write config "mod" = NOISY_MODE ("mod" does not exist, so an error is expected).
- Write config threshold_mc = 45000.
- Write config mode = RAMP_MODE.
- Write config sampling_mc = 1 (1ms).
- Obtain the number of samples in 1 second with count = print_sample_test(4, ep, fd).
- If count is between 900 and 1100 and write config function returned -EINVAL when wrong config "mod" was used, the test is passed.

**T5 — Concurrency:** run reader + config writer concurrently; no deadlocks; safe unload.
- Create a thread named thread_read that reads the configs sampling_mc, threshold_mc and mode 5 times each.
- Create a thread named thread_write that writes the configs samling_mc, threshold_mc and mode 5 times each.
- Start thread thread_read.
- Start thread thread_write.
- Join thread_read (use timeout=2 to detect deadlocks if it does not finish in time).
- Join thread_write (use timeout=2 to detect deadlocks if it does not finish in time).
- Assert if thread_read is still aive.
- Assert if thread_write is still alive.
- If no deadlocks were found and the threads are not alive, the test is passed.

**T6 — API Contract:** struct size/endianness documented; user app handles partial reads.
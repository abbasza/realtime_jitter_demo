# rt_jitter_demo

Two tiny, self-contained C programs that both try to run a 1 kHz loop and
print, cycle by cycle, exactly when they actually "sent" (a stand-in for
handing a frame to the EtherCAT/bus layer). One uses the naive approach,
the other uses the real-time techniques described below. Run both and
compare the jitter numbers at the bottom.

- `src/baseline.c`   — `SCHED_OTHER` (default scheduler), relative `usleep(1000)`
- `src/rt_version.c` — `SCHED_FIFO`, `mlockall()`, absolute-deadline
  `clock_nanosleep()`, plus a lock-free triple buffer standing in for the
  hand-off to the comms/serialization thread

## Build

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

This produces `build/baseline` and `build/rt_version`.

## Run

```bash
./build/baseline 2000        # 2000 cycles = ~2 seconds
./build/rt_version 2000
```

Each line printed is one cycle:

```
  iter   expected_us    actual_us   jitter_us
```

`expected_us` is where cycle N *should* land if the loop were perfect;
`actual_us` is where it actually landed; `jitter_us` is the difference.
A summary block (min/max/mean/stddev/peak-to-peak jitter) prints at the end.

For a quick side-by-side without the full per-cycle log:

```bash
./scripts/compare.sh 2000
```

## What to expect

Numbers below are from a single run on an ordinary (non-RT) Linux
sandbox with no CPU isolation — i.e. the worst case, with no kernel or
system tuning applied at all. Your numbers will vary by machine, and
will look *much* better on real hardware with the tuning steps below.

```
BASELINE  (2000 cycles @ 1 kHz)
  min jitter    :    463.4 us
  max jitter    : 216979.6 us
  mean jitter   : 106438.6 us
  stddev jitter :  62439.8 us
  peak-to-peak  : 216516.2 us

IMPROVED  (2000 cycles @ 1 kHz)
  min jitter    :     14.4 us
  max jitter    :    494.0 us
  mean jitter   :     45.8 us
  stddev jitter :     26.3 us
  peak-to-peak  :    479.6 us
```

Even with no root and no RT kernel, `clock_nanosleep(TIMER_ABSTIME)`
alone (no drift accumulation) beats relative `usleep` by orders of
magnitude, because `usleep`'s error compounds every single cycle.

## Getting the *real* benefit (matters for the target board, not just this demo)

The code will run without any of this — it degrades gracefully and
prints a warning — but to actually see sub-100 µs worst-case jitter on
your Jetson/RPi you need:

1. **A `PREEMPT_RT` patched kernel** (stock Linux is not a real-time OS).
2. **Run with real-time privilege**, e.g.:
   ```bash
   sudo chrt -f 80 ./build/rt_version 2000
   ```
   or grant the binary `CAP_SYS_NICE` instead of running as root.
3. **Isolate a CPU core** for the loop: `isolcpus`, `nohz_full`,
   `irqaffinity` kernel boot params, then pin the process to that core
   (`taskset` or `sched_setaffinity`, not included in this minimal demo).
4. **Baseline the OS itself first** with `cyclictest` (from
   `rt-tests`) before trusting any application-level number — it tells
   you the floor your hardware/kernel combo can hit before your code
   even runs.

## Files

```
CMakeLists.txt
src/
  common.h       - shared timing helpers + the triple buffer
  baseline.c     - naive version
  rt_version.c   - RT-scheduled version
scripts/
  compare.sh     - runs both, prints summaries side by side
```

## Extending this toward the real system

- Swap the `printf` "send" in `rt_version.c` for the actual EtherCAT
  PDO exchange call (SOEM/IgH) — that's your real bottleneck, so once
  this skeleton is solid, wrap *that* call with the same timestamping
  to see how much of your budget it eats.
- Use EtherCAT Distributed Clocks (DC) to sync the whole bus to this
  loop, not just your local wake-up time — DC sync is what actually
  bounds jitter network-wide, not just on the master.
- Keep this triple buffer for "always-want-latest" data (position
  commands); if you also need "must-not-drop-any-sample" data (e.g. a
  log stream), that needs a different structure (ring buffer/queue),
  not a triple buffer.

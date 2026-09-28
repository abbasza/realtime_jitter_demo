/* rt_version.c
 *
 * Same 1 kHz loop, three changes:
 *   1. SCHED_FIFO real-time priority (needs CAP_SYS_NICE / root, and a
 *      PREEMPT_RT kernel to really shine).
 *   2. mlockall() so no page fault ever steals time from the loop.
 *   3. clock_nanosleep(TIMER_ABSTIME) against a monotonically
 *      incrementing absolute deadline, instead of a relative sleep,
 *      so error does not accumulate cycle over cycle.
 * Also demonstrates the lock-free triple buffer that would sit between
 * this thread and the thread that serializes onto EtherCAT.
 *
 * NOTE: without root/RT kernel this still *runs*, it just won't show
 * the full benefit -- see the warning it prints if SCHED_FIFO fails.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sched.h>
#include <sys/mman.h>
#include <math.h>
#include "common.h"

#define DEFAULT_ITERATIONS 2000
#define RT_PRIORITY 80  /* leave headroom below kernel-critical prios (usually <99) */

static int setup_rt(void) {
    struct sched_param sp;
    memset(&sp, 0, sizeof(sp));
    sp.sched_priority = RT_PRIORITY;

    int rc_sched = sched_setscheduler(0, SCHED_FIFO, &sp);
    if (rc_sched != 0) {
        fprintf(stderr,
            "[warn] sched_setscheduler(SCHED_FIFO) failed: %s\n"
            "       Run as root (or grant CAP_SYS_NICE) on a PREEMPT_RT\n"
            "       kernel to see the real jitter improvement. Continuing\n"
            "       on the default scheduler for this demo run.\n",
            strerror(errno));
    }

    int rc_lock = mlockall(MCL_CURRENT | MCL_FUTURE);
    if (rc_lock != 0) {
        fprintf(stderr, "[warn] mlockall failed: %s (page faults may add jitter)\n",
                strerror(errno));
    }

    return (rc_sched == 0);
}

int main(int argc, char **argv) {
    int iterations = (argc > 1) ? atoi(argv[1]) : DEFAULT_ITERATIONS;

    printf("=== IMPROVED (SCHED_FIFO + mlockall + absolute clock_nanosleep + triple buffer) ===\n");
    int rt_ok = setup_rt();
    printf("RT scheduling active: %s\n", rt_ok ? "yes" : "no (see warning above)");
    printf("target period: 1000 us  |  iterations: %d\n", iterations);
    printf("%6s  %12s  %12s  %10s\n", "iter", "expected_us", "actual_us", "jitter_us");

    triple_buffer_t tb;
    tb_init(&tb);

    double *jitter_us = calloc((size_t)iterations, sizeof(double));
    if (!jitter_us) { perror("malloc"); return 1; }

    struct timespec start, next, now;
    clock_gettime(CLOCK_MONOTONIC, &start);
    next = start;
    uint32_t seq = 0;

    for (int i = 0; i < iterations; i++) {
        timespec_add_ns(&next, PERIOD_NS);
        /* Sleep to an absolute deadline: no drift accumulation, and the
         * scheduler can plan around a known wake-up time instead of us
         * re-arming a relative timer every cycle. */
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);

        clock_gettime(CLOCK_MONOTONIC, &now);
        int64_t elapsed_ns  = timespec_diff_ns(&now, &start);
        double  expected_us = (double)(i + 1) * 1000.0;
        double  actual_us   = (double)elapsed_ns / 1000.0;
        double  j           = actual_us - expected_us;
        jitter_us[i] = j;

        /* --- lock-free triple-buffer publish: no malloc, no mutex --- */
        control_frame_t frame;
        frame.timestamp_s = now.tv_sec + now.tv_nsec / 1e9;
        frame.seq         = seq++;
        frame.position    = 0.0f;  /* placeholder for real control output */
        frame.velocity    = 0.0f;
        tb_write(&tb, &frame);

        /* Simulated "bridge send" -- in the real system this is where
         * the EtherCAT PDO exchange happens; here we just print at the
         * instant of send so a reviewer can see it land on schedule. */
        (void)tb_read(&tb); /* stand-in for the consumer picking it up */
        printf("%6d  %12.1f  %12.1f  %+10.1f\n", i, expected_us, actual_us, j);
    }

    double min = jitter_us[0], max = jitter_us[0], sum = 0.0;
    for (int i = 0; i < iterations; i++) {
        if (jitter_us[i] < min) min = jitter_us[i];
        if (jitter_us[i] > max) max = jitter_us[i];
        sum += jitter_us[i];
    }
    double mean = sum / iterations;
    double sq = 0.0;
    for (int i = 0; i < iterations; i++) {
        double d = jitter_us[i] - mean;
        sq += d * d;
    }
    double stddev = sqrt(sq / iterations);

    printf("\n--- IMPROVED SUMMARY (%d cycles @ 1 kHz) ---\n", iterations);
    printf("min jitter    : %8.1f us\n", min);
    printf("max jitter    : %8.1f us\n", max);
    printf("mean jitter   : %8.1f us\n", mean);
    printf("stddev jitter : %8.1f us\n", stddev);
    printf("peak-to-peak  : %8.1f us\n", max - min);

    free(jitter_us);
    return 0;
}

/* baseline.c
 *
 * Naive 1 kHz loop: SCHED_OTHER (default), relative usleep(1000).
 * This is the "v0" version — no RT priority, drift accumulates because
 * we sleep relative to "now" instead of an absolute deadline, and any
 * other process on the box can delay us. Run this first to get a
 * reference jitter number, then compare against rt_version.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include "common.h"

#define DEFAULT_ITERATIONS 2000

int main(int argc, char **argv) {
    int iterations = (argc > 1) ? atoi(argv[1]) : DEFAULT_ITERATIONS;

    printf("=== BASELINE  (SCHED_OTHER, relative sleep, no triple buffer) ===\n");
    printf("target period: 1000 us  |  iterations: %d\n", iterations);
    printf("%6s  %12s  %12s  %10s\n", "iter", "expected_us", "actual_us", "jitter_us");

    double *jitter_us = calloc((size_t)iterations, sizeof(double));
    if (!jitter_us) { perror("malloc"); return 1; }

    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < iterations; i++) {
        usleep(1000); /* relative sleep: OS is free to wake us "whenever" */

        clock_gettime(CLOCK_MONOTONIC, &now);
        int64_t elapsed_ns   = timespec_diff_ns(&now, &start);
        double  expected_us  = (double)(i + 1) * 1000.0;
        double  actual_us    = (double)elapsed_ns / 1000.0;
        double  j            = actual_us - expected_us;
        jitter_us[i] = j;

        /* This printf is standing in for "the moment the frame would
         * have been handed to the bus/comms layer". */
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

    printf("\n--- BASELINE SUMMARY (%d cycles @ 1 kHz) ---\n", iterations);
    printf("min jitter    : %8.1f us\n", min);
    printf("max jitter    : %8.1f us\n", max);
    printf("mean jitter   : %8.1f us\n", mean);
    printf("stddev jitter : %8.1f us\n", stddev);
    printf("peak-to-peak  : %8.1f us\n", max - min);

    free(jitter_us);
    return 0;
}

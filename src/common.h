#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <time.h>
#include <stdatomic.h>

/* 1 kHz control loop */
#define PERIOD_NS   1000000L
#define BUFFER_SIZE 3

/* This is a stand-in for whatever you actually serialize onto the bus
 * (EtherCAT PDO, CAN frame, etc). Keep it fixed-size / fixed-layout so
 * there is zero allocation or general-purpose serialization on the
 * hot path. */
typedef struct {
    double   timestamp_s;
    float    position;
    float    velocity;
    uint32_t seq;
} control_frame_t;

/* Single-producer / single-consumer triple buffer.
 * Writer always writes into the slot that is NOT currently "latest",
 * then atomically publishes it. Reader always reads whatever slot is
 * currently "latest". No locks, no blocking, reader always gets the
 * newest complete frame and never tears a partial write. */
typedef struct {
    control_frame_t slots[BUFFER_SIZE];
    atomic_int latest_index;
} triple_buffer_t;

static inline void tb_init(triple_buffer_t *tb) {
    atomic_init(&tb->latest_index, 0);
}

static inline void tb_write(triple_buffer_t *tb, const control_frame_t *frame) {
    int cur  = atomic_load_explicit(&tb->latest_index, memory_order_relaxed);
    int next = (cur + 1) % BUFFER_SIZE;
    tb->slots[next] = *frame;                                   /* fill off-screen slot */
    atomic_store_explicit(&tb->latest_index, next, memory_order_release); /* publish */
}

static inline control_frame_t tb_read(triple_buffer_t *tb) {
    int idx = atomic_load_explicit(&tb->latest_index, memory_order_acquire);
    return tb->slots[idx];
}

static inline int64_t timespec_diff_ns(const struct timespec *a, const struct timespec *b) {
    return (int64_t)(a->tv_sec - b->tv_sec) * 1000000000LL + (a->tv_nsec - b->tv_nsec);
}

static inline void timespec_add_ns(struct timespec *t, long ns) {
    t->tv_nsec += ns;
    while (t->tv_nsec >= 1000000000L) {
        t->tv_nsec -= 1000000000L;
        t->tv_sec++;
    }
}

#endif /* COMMON_H */

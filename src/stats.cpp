#include "../include/stats.h"

time_t getNsTimestamp() {
    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);
    return tp.tv_sec * 1e9 + tp.tv_nsec;
}

time_t getCycleCount() {
    return __rdtsc();
}

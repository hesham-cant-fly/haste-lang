#ifndef MY_TIMING_H
#define MY_TIMING_H

#include <time.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct timer {
	struct timespec start;
	struct timespec end;
};

static inline void timer_start(struct timer *t)
{
	timespec_get(&t->start, TIME_UTC);
}

static inline void timer_stop(struct timer *t)
{
	timespec_get(&t->end, TIME_UTC);
}

static inline double timer_elapsed_ns(struct timer *t)
{
	return (double)(t->end.tv_sec - t->start.tv_sec) * 1e9
	     + (double)(t->end.tv_nsec - t->start.tv_nsec);
}

static inline double timer_elapsed_us(struct timer *t)
{
	return timer_elapsed_ns(t) / 1e3;
}

static inline double timer_elapsed_ms(struct timer *t)
{
	return timer_elapsed_ns(t) / 1e6;
}

static inline double timer_elapsed_s(struct timer *t)
{
	return timer_elapsed_ns(t) / 1e9;
}

enum time_unit { TU_NS, TU_US, TU_MS, TU_S };

static inline const char *time_unit_str(enum time_unit u)
{
	switch (u) {
	case TU_NS: return "ns";
	case TU_US: return "\xC2\xB5s";  // µs in UTF-8
	case TU_MS: return "ms";
	case TU_S:  return "s";
	}
	return "?";
}

struct timed_result {
	double value;
	enum time_unit unit;
};

static inline struct timed_result format_duration(double ns)
{
	if (ns < 1e3) return (struct timed_result){ ns,       TU_NS };
	if (ns < 1e6) return (struct timed_result){ ns / 1e3, TU_US };
	if (ns < 1e9) return (struct timed_result){ ns / 1e6, TU_MS };
	return (struct timed_result){ ns / 1e9, TU_S };
}

static inline void print_timing_report(struct timer timers[], const char *names[], int count)
{
	double totals_ns[count];
	double grand_total_ns = 0;

	for (int i = 0; i < count; i++) {
		totals_ns[i] = timer_elapsed_ns(&timers[i]);
		grand_total_ns += totals_ns[i];
	}

	int name_width = 0;
	for (int i = 0; i < count; i++) {
		int w = 0;
		for (const char *p = names[i]; *p; p++) w++;
		if (w > name_width) name_width = w;
	}

	struct timed_result gt = format_duration(grand_total_ns);

	fprintf(stderr, "\n  timing report\n");
	fprintf(stderr, "  ─────────────\n");

	for (int i = 0; i < count; i++) {
		struct timed_result r = format_duration(totals_ns[i]);
		double pct = grand_total_ns > 0 ? totals_ns[i] / grand_total_ns * 100 : 0;
		fprintf(stderr, "  %-*s took %.2f %-2s (%05.2f%%)\n",
			name_width, names[i], r.value, time_unit_str(r.unit), pct);
	}

	fprintf(stderr, "  ─────────────\n");
	fprintf(stderr, "  %-*s took %.2f %s (100.00%%)\n",
		name_width, "total", gt.value, time_unit_str(gt.unit));
}

#ifdef __cplusplus
}
#endif

#endif // MY_TIMING_H

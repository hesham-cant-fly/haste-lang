#ifndef MY_TIMING_H_
#define MY_TIMING_H_

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include "my_allocator.h"
#include "my_managed_array.h"

#ifdef __cplusplus
extern "C" {
#endif

struct timer {
	const char *name;
	size_t total_allocated_memory;
	struct timespec start;
	struct timespec end;
};

struct timer_list {
	struct Allocator allocator;
	size_t len;
	size_t cap;
	struct timer *items;
};

static inline void timer_start(
	struct timer_list *self,
	const char *name)
{
	struct timer result = {
		.name = name,
	};
	timespec_get(&result.start, TIME_UTC);
	marrpush(*self, result);
}

static inline void timer_stop(struct timer_list *self, size_t total_allocated_memory)
{
	assert(self->len > 0);
	timespec_get(&self->items[self->len - 1].end, TIME_UTC);
	self->items[self->len - 1].total_allocated_memory = total_allocated_memory;
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

static inline void print_timing_report(struct timer_list timers)
{
	double totals_ns[timers.len];
	double grand_total_ns = 0;

	for (size_t i = 0; i < timers.len; i++) {
		totals_ns[i] = timer_elapsed_ns(&timers.items[i]);
		grand_total_ns += totals_ns[i];
	}

	int name_width = 0;
	for (size_t i = 0; i < timers.len; i++) {
		int w = strlen(timers.items[i].name);
		if (w > name_width) name_width = w;
	}

	struct timed_result gt = format_duration(grand_total_ns);

	fprintf(stderr, "\n  timing report\n");
	fprintf(stderr, "  ─────────────\n");

	for (size_t i = 0; i < timers.len; i++) {
		struct timed_result r = format_duration(totals_ns[i]);
		double pct = grand_total_ns > 0 ? totals_ns[i] / grand_total_ns * 100 : 0;

		double size = (double)timers.items[i].total_allocated_memory;
		const char *unit = "B";
		if (size >= 1024) { size /= 1024; unit = "kB"; }
		if (size >= 1024) { size /= 1024; unit = "MB"; }
		if (size >= 1024) { size /= 1024; unit = "GB"; }

		fprintf(stderr, "  %-*s took %.2f %-2s (%05.2f%%) allocated (%.2f %s)\n",
				name_width, timers.items[i].name, r.value, time_unit_str(r.unit), pct,
				size, unit);
	}

	fprintf(stderr, "  ─────────────\n");
	fprintf(stderr, "  %-*s took %.2f %s (100.00%%)\n",
		name_width, "total", gt.value, time_unit_str(gt.unit));
}

#ifdef __cplusplus
}
#endif

#endif // MY_TIMING_H_

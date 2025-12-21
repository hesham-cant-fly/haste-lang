#ifndef __CONFIG_H
#define __CONFIG_H

#include "error.h"
#include "tir.h"

#define MAX_FUNCTION_ARGS 256

typedef error (*TargetFN)(const struct Tir tir);

struct Backend {
	const char* name;
	TargetFN fn;
};

extern const struct Backend backends[];

#endif // !__CONFIG_H

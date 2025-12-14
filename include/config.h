#ifndef __CONFIG_H
#define __CONFIG_H

#include "error.h"
#include "tir.h"

#define MAX_FUNCTION_ARGS 256

typedef error (*TargetFN)(const Tir tir);

typedef struct Backend {
	const char* name;
	TargetFN fn;
} Backend;

extern const Backend backends[];

#endif // !__CONFIG_H

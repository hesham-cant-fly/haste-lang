#ifndef __ANALYSIS_H
#define __ANALYSIS_H

#include "error.h"
#include "hir.h"
#include "tir.h"

error analyze_hir(Hir hir, Tir* out);

#endif // !__ANALYSIS_H

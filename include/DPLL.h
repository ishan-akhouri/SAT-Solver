#ifndef DPLL_H
#define DPLL_H

#include "SATInstance.h"

// Global counters for recursive calls and backtracks
extern int dpll_calls;
extern int backtracks;

// Optimizations
bool unitPropagation(SATInstance& instance);
void pureLiteralElimination(SATInstance& instance);

// Main DPLL algorithm
bool DPLL(SATInstance& instance);

#endif
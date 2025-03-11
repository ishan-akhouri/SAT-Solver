#ifndef DPLL_H
#define DPLL_H

#include "SATInstance.h"

extern int dpll_calls;
extern int backtracks;

bool unitPropagation(SATInstance& instance);
void pureLiteralElimination(SATInstance& instance);
bool DPLL(SATInstance& instance);

#endif
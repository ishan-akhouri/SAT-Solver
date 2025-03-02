#ifndef DPLL_H
#define DPLL_H

#include "SATInstance.h"

bool unitPropagation(SATInstance& instance);
bool DPLL(SATInstance& instance);

#endif 
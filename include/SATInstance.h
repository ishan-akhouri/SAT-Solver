#ifndef SAT_INSTANCE_H
#define SAT_INSTANCE_H

#include <vector>
#include <unordered_map>
#include <iostream>

using Clause = std::vector<int>;  // A clause is a disjunction (OR) of literals
using CNF = std::vector<Clause>;  // A CNF formula is a conjunction (AND) of clauses

struct SATInstance {
    CNF formula;                             // The formula in CNF form
    std::unordered_map<int, bool> assignments; // Variable assignments (var → value)

    SATInstance(const CNF &f) : formula(f) {}

    void print() {
        std::cout << "SAT Problem in CNF:\n";
        for (const auto& clause : formula) {
            for (int literal : clause) {
                std::cout << (literal > 0 ? "x" : "~x") << abs(literal) << " ";
            }
            std::cout << "\n";
        }
    }
};

#endif
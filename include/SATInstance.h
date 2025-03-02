#ifndef SAT_INSTANCE_H
#define SAT_INSTANCE_H

#include <vector>
#include <unordered_map>
#include <iostream>

// This file defines a data structure to represent a Boolean SAT problem in CNF form, with support for storing variable assignments and printing the formula.

using Clause = std::vector<int>; // Clause is a vector of integers, where each integer represents a literal
using CNF = std::vector<Clause>;// CNF is a vector of clauses, which together form the CNF formula

struct SATInstance // Represents a SAT problem in Conjunctive Normal Form (CNF)
{
    CNF formula; // Stores the formula as a list of clauses 
    std::unordered_map<int, bool> assignments; //Hashmap where keys are variable numbers, values are boolean values representing their assignments

    SATInstance(const CNF &f) : formula(f) {} // Initialize formula with f

    void print() 
    {
        std::cout << "SAT Problem in CNF:\n";
        for (const auto& clause : formula) // Loop through clauses
        {
            for (int literal : clause) // Loop through each literal in the clause, print it
            {
                std::cout << (literal > 0 ? "x" : "~x") << abs(literal) << " ";
            }
            std::cout << "\n";
        }
    }
};

#endif


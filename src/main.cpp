#include <iostream>
#include "../include/SATInstance.h"
#include "../include/DPLL.h" 
#include <chrono>

int main()
{
    std::cout << "SAT Solver Starting..." << std::endl; //Indicate that the SAT Solver is starting
    CNF exampleCNF = {{1, 2}, {-1, 3}, {-2, -3}}; //Define CNF formula as a vector of clauses

    SATInstance instance(exampleCNF); // Create an SATInstance object and initialize it with exampleCNF
    instance.print(); //Display CNF formula before solving

    auto start = std::chrono::high_resolution_clock::now();
    bool result = DPLL(instance); //Solve the SAT problem, store it in result variable
    auto end = std::chrono::high_resolution_clock::now();

    std::cout << (result ? "SATISFIABLE" : "UNSATISFIABLE") << std::endl; //Print whether the formula is SATISFIABLE or UNSATISFIABLE

    std::chrono::duration<double, std::milli> elapsed = end - start;
    std::cout << "Execution Time: " << elapsed.count() << " ms " << std::endl;
    std::cout << "Recursive Calls: " << dpll_calls << std::endl;
    std::cout << "Backtracks: " << backtracks << std::endl;
    return 0;
}
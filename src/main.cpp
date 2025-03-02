#include <iostream>
#include "../include/SATInstance.h"
#include "../include/DPLL.h" 

int main()
{
    std::cout << "SAT Solver Starting..." << std::endl; //Indicate that the SAT Solver is starting
    CNF exampleCNF = {{1, 2}, {-1, 3}, {-2, -3}}; //Define CNF formula as a vector of clauses

    SATInstance instance(exampleCNF); // Create an SATInstance object and initialize it with exampleCNF
    instance.print(); //Display CNF formula before solving

    bool result = DPLL(instance); //Solve the SAT problem, store it in result variable
    std::cout << (result ? "SATISFIABLE" : "UNSATISFIABLE") << std::endl; //Print whether the formula is SATISFIABLE or UNSATISFIABLE
    return 0;
}
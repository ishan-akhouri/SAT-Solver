#include <iostream>
#include "../include/SATInstance.h"
#include "../include/DPLL.h" 

int main()
{
    std::cout << "SAT Solver Starting..." << std::endl;
    CNF exampleCNF = {{1, 2}, {-1, 3}, {-2, -3}};

    SATInstance instance(exampleCNF);
    instance.print();

    bool result = DPLL(instance);
    std::cout << (result ? "SATISFIABLE" : "UNSATISFIABLE") << std::endl;
    return 0;
    return 0;
}
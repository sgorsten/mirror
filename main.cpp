#include "refl.h"

#include <iostream>

float add(float a, float b) { return a+b; }
float mul(float a, float b) { return a*b; }
void print(float x) { std::cout << x << std::endl; }

int main()
{
    auto prog = std::make_shared<const Program>(2, std::vector<Line>{
        {Function::Bind(mul,"mul"), {-1,-1}},
        {Function::Bind(add,"add"), {-2, 0}},
        {Function::Bind(print,"print"), {1}},
    });
    std::cout << *prog << std::endl;
    
    const auto ev = Event<float,float>(prog);

    ev(3,2);

    return 0;
}
#include "graph.h"

std::ostream & operator << (std::ostream & out, const Line & line)
{ 
    out << line.func->GetName() << '(';
    for(size_t i=0; i<line.args.size(); ++i)
    {
        if(i) out << ", ";
        auto arg = line.args[i];
        if(arg < 0) out << "arg" << -arg-1;
        else out << "tmp" << arg;
    }
    return out << "); // " << *line.func;
}

std::ostream & operator << (std::ostream & out, const Program & prog)
{
    out << prog.argCount << " args:" << std::endl;
    for(size_t i=0; i<prog.lines.size(); ++i)
    {
        out << "  ";
        if(prog.lines[i].func->GetReturnType().type->index != typeid(void)) out << "tmp" << i << " = ";
        out << prog.lines[i] << std::endl;
    }
    return out;
}
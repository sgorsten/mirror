#include "refl.h"

std::ostream & operator << (std::ostream & out, const VarType & vt)
{ 
    out << vt.type->type.name();
    if(vt.isConst) out << " const";
    if(vt.isVolatile) out << " volatile";
    switch(vt.indirection)
    {
    case VarType::LValueRef: out << " &"; break;
    case VarType::RValueRef: out << " &&"; break;
    }
    return out;
}

std::ostream & operator << (std::ostream & out, const Function & f)
{ 
    out << f.type->returnType << ' ' << f.name << '(';
    for(size_t i=0; i<f.type->paramTypes.size(); ++i)
    {
        if(i) out << ',';
        out << f.type->paramTypes[i];
    }
    return out << ')';
}

std::ostream & operator << (std::ostream & out, const Line & line)
{ 
    out << line.func.name << '(';
    for(size_t i=0; i<line.args.size(); ++i)
    {
        if(i) out << ", ";
        auto arg = line.args[i];
        if(arg < 0) out << "arg" << -arg-1;
        else out << "tmp" << arg;
    }
    return out << "); // " << line.func;
}

std::ostream & operator << (std::ostream & out, const Program & prog)
{
    out << prog.argCount << " args:" << std::endl;
    for(size_t i=0; i<prog.lines.size(); ++i)
    {
        out << "  ";
        if(prog.lines[i].func.type->returnType.type->type != typeid(void)) out << "tmp" << i << " = ";
        out << prog.lines[i] << std::endl;
    }
    return out;
}
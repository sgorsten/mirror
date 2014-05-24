#include "refl.h"

std::shared_ptr<void> Type::DefaultConstruct() const
{
    assert(IsDefaultConstructible());
    if(IsTrivial()) return std::shared_ptr<void>(std::malloc(size), std::free); // Manage trivial types with malloc and free
    else return nonTrivialOps->defConstruct(); // If the type has a constructor, invoke it
}

std::shared_ptr<void> Type::CopyConstruct(const void * r) const
{
    assert(IsCopyConstructible());
    if(IsTrivial())
    {
        auto obj = DefaultConstruct();
        CopyAssign(obj.get(), r);
        return obj;
    }
    else return nonTrivialOps->copyConstruct(r);
}

std::shared_ptr<void> Type::MoveConstruct(void * r) const
{
    assert(IsMoveConstructible());
    if(IsTrivial()) return CopyConstruct(r);
    else return nonTrivialOps->moveConstruct(r);
}

void Type::CopyAssign(void * l, const void * r) const
{
    assert(IsCopyAssignable());
    if(IsTrivial()) memcpy(l, r, size);
    else nonTrivialOps->copyAssign(l, r);
}

void Type::MoveAssign(void * l, void * r) const
{
    assert(IsMoveAssignable());
    if(IsTrivial()) CopyAssign(l, r);
    else nonTrivialOps->moveAssign(l, r);
}

std::ostream & operator << (std::ostream & out, const Type & type)
{
    switch(type.kind)
    {
    default: assert(false); return out;
    case Type::Fundamental: return out << type.index.name();
    case Type::Class: return out << type.index.name();
    case Type::Union: return out << type.index.name();
    case Type::Enum: return out << type.index.name();
    case Type::Array:
        {
            std::vector<size_t> dims;
            auto t = &type;
            while(t->kind == Type::Array)
            {
                dims.push_back(t->size / t->elementType->size);
                t = t->elementType;
            }
            out << *t;
            for(auto dim : dims) out << '[' << dim << ']';
        }
        return out;
    case Type::Pointer:
        if(type.elementType->kind == Type::Function) // Pointer to function
        {
            out << type.elementType->returnType << '(';
            if(type.classType) out << *type.classType << "::"; // Member function pointer
            out << "*)(";
            for(size_t firstParam=type.classType?1:0, i=firstParam; i<type.elementType->paramTypes.size(); ++i) // For each parameter (skip first param if a member function pointer)
            {
                if(i > firstParam) out << ", ";
                out << type.elementType->paramTypes[i];
            }
            out << ')';
            if(type.classType)
            {
                if(type.elementType->paramTypes[0].isConst) out << " const";
                if(type.elementType->paramTypes[0].isVolatile) out << " volatile";
            }
            return out;
        }
        else // Pointer to data
        {
            out << *type.elementType;
            if(type.isPointeeConst) out << " const";
            if(type.isPointeeVolatile) out << " volatile";
            out << ' ';
            if(type.classType) out << *type.classType << "::"; // Member variable ptr
            return out << '*';
        }
    case Type::Function:
        out << type.returnType << '(';
        for(size_t i=0; i<type.paramTypes.size(); ++i)
        {
            if(i > 0) out << ", ";
            out << type.paramTypes[i];
        }
        return out << ')';
    }
}

std::ostream & operator << (std::ostream & out, const VarType & vt)
{ 
    out << *vt.type;
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
    out << f.GetReturnType() << ' ' << f.GetName() << '(';
    for(size_t i=0; i<f.GetParamTypes().size(); ++i)
    {
        if(i) out << ',';
        out << f.GetParamTypes()[i];
    }
    return out << ')';
}
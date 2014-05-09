#include <cassert>
#include <vector>
#include <functional>
#include <memory>
#include <typeindex>
#include <string>
#include <ostream>

struct VarType
{
    enum                                Indirection                 { None, LValueRef, RValueRef };
    std::type_index                     type;
    bool                                isConst;
    bool                                isVolatile;
    Indirection                         indirection;    

                                        VarType()                   : type(typeid(void)), isConst(), isVolatile(), indirection() {}
    template<class T>                   VarType(std::tuple<T> *)    : type(typeid(T)), isConst(std::is_const<T>::value), isVolatile(std::is_volatile<T>::value), indirection(std::is_lvalue_reference<T>::value ? LValueRef : std::is_lvalue_reference<T>::value ? RValueRef : None) {}

    template<class T> static VarType    Get()                       { return VarType((std::tuple<T> *)nullptr); }
};
std::ostream & operator << (std::ostream & out, const VarType & vt)
{ 
    out << vt.type.name();
    if(vt.isConst) out << " const";
    if(vt.isVolatile) out << " volatile";
    switch(vt.indirection)
    {
    case VarType::LValueRef: out << " &";
    case VarType::RValueRef: out << " &&";
    }
    return out;
}

struct Function
{
    std::function<std::shared_ptr<void>(void **)> impl;
    std::vector<VarType>                params;
    VarType                             result;
    std::string                         name;

    std::shared_ptr<void>               Invoke(void * args[]) const { return impl(args); }
};
std::ostream & operator << (std::ostream & out, const Function & f)
{ 
    out << f.result << ' ' << f.name << '(';
    for(size_t i=0; i<f.params.size(); ++i)
    {
        if(i) out << ',';
        out << f.params[i];
    }
    return out << ')';
}

struct Line
{
    Function func;
    std::vector<int> args;
};
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

class Program
{
    size_t argCount;
    std::vector<Line> lines;
public:
    Program(size_t argCount, std::vector<Line> _lines) : argCount(argCount), lines(move(_lines))
    {
        for(size_t i=0; i<lines.size(); ++i)
        {
            assert(lines[i].args.size() <= 4);
            for(int arg : lines[i].args)
            {
                assert(arg >= -(int)argCount && arg < (int)i);
            }
        }
    }

    size_t GetArgumentCount() const { return argCount; }

    std::shared_ptr<void> Execute(const std::vector<void *> & args) const
    {
        std::vector<std::shared_ptr<void>> results;
        for(auto & line : lines)
        {
            void * callArgs[4];
            for(size_t i=0; i<line.args.size(); ++i)
            {
                callArgs[i] = line.args[i] < 0 ? args[-line.args[i]-1] : results[line.args[i]].get();
            }
            results.push_back(line.func.Invoke(callArgs));
        }
        return results.back();
    }

    friend std::ostream & operator << (std::ostream & out, const Program & prog)
    {
        out << prog.argCount << " args:" << std::endl;
        for(size_t i=0; i<prog.lines.size(); ++i)
        {
            out << "  ";
            if(prog.lines[i].func.result.type != typeid(void))
            {
                if(i+1 < prog.lines.size()) out << "tmp" << i << " = ";
                else out << "return ";
            }
            out << prog.lines[i] << std::endl;
        }
        return out;
    }
};

template<class T> T &  PassArg(void * addr, std::tuple<T & > *) { return             *reinterpret_cast<T *>(addr);   } // For lvalue reference types, just pass a ref to the object
template<class T> T && PassArg(void * addr, std::tuple<T &&> *) { return   std::move(*reinterpret_cast<T *>(addr));  } // For rvalue reference types, pass an rvalue ref to the object
template<class T> T    PassArg(void * addr, std::tuple<T   > *) { return T(std::move(*reinterpret_cast<T *>(addr))); } // For value types, move-construct a new value from the original
template<class T> T    PassArg(void * addr) { return PassArg(addr, (std::tuple<T> *)nullptr); }

// CallWithArgs takes a function object of type F, and an array of void pointers, and a function pointer indicating the call signature, and casts the pointers to the appropriate argument types
template<class F, class R                                    > R CallWithArgs(F func, void * args[], R(*)(       )) { return func(                                                                                  ); }
template<class F, class R, class A                           > R CallWithArgs(F func, void * args[], R(*)(A      )) { return func(PassArg<A>(args[0])                                                               ); }
template<class F, class R, class A, class B                  > R CallWithArgs(F func, void * args[], R(*)(A,B    )) { return func(PassArg<A>(args[0]), PassArg<B>(args[1])                                          ); }
template<class F, class R, class A, class B, class C         > R CallWithArgs(F func, void * args[], R(*)(A,B,C  )) { return func(PassArg<A>(args[0]), PassArg<B>(args[1]), PassArg<C>(args[2])                     ); }
template<class F, class R, class A, class B, class C, class D> R CallWithArgs(F func, void * args[], R(*)(A,B,C,D)) { return func(PassArg<A>(args[0]), PassArg<B>(args[1]), PassArg<C>(args[2]), PassArg<D>(args[3])); }

template<class P, class... PS> void InitParameterList(std::vector<VarType> & params, std::tuple<P,PS...> *) { params.push_back(VarType::Get<P>()); InitParameterList(params, (std::tuple<PS...> *)nullptr); }
                               void InitParameterList(std::vector<VarType> & params, std::tuple<       > *) {}

// BindFunctionResult takes a function object of type F, and produces a generic function, translating the return type to the most appropriate representation
template<class F, class R, class... P> Function BindFunctionResult(F func, R   (*)(P...)) { Function f; f.result = VarType::Get<R   >(); InitParameterList(f.params, (std::tuple<P...> *)nullptr); f.impl = [func](void * args[]) { return std::make_shared<R>(CallWithArgs(func, args, (R(*)(P...))nullptr)); }; return f; }
template<class F, class R, class... P> Function BindFunctionResult(F func, R & (*)(P...)) { Function f; f.result = VarType::Get<R & >(); InitParameterList(f.params, (std::tuple<P...> *)nullptr); f.impl = [func](void * args[]) { return std::shared_ptr<void>(&CallWithArgs(func, args, (R & (*)(P...))nullptr)); }; return f; }
template<class F, class R, class... P> Function BindFunctionResult(F func, R &&(*)(P...)) { Function f; f.result = VarType::Get<R &&>(); InitParameterList(f.params, (std::tuple<P...> *)nullptr); f.impl = [func](void * args[]) { return std::shared_ptr<void>(&CallWithArgs(func, args, (R &&(*)(P...))nullptr)); }; return f; }
template<class F,          class... P> Function BindFunctionResult(F func, void(*)(P...)) { Function f; f.result = VarType::Get<void>(); InitParameterList(f.params, (std::tuple<P...> *)nullptr); f.impl = [func](void * args[]) { CallWithArgs(func, args, (void(*)(P...))nullptr); return std::shared_ptr<void>(); }; return f; }

// BindFunction accepts pointers to functions or member functions and returns a generic function. Member functions are transformed into free functions taking an extra first parameter.
template<         class R, class... P> Function BindFunction(R (   *func)(P...)               ) { return BindFunctionResult( func,                                                                              (R(*)(                    P...))nullptr); }
template<class C, class R, class... P> Function BindFunction(R (C::*func)(P...)               ) { return BindFunctionResult([func](               C & c, P... p) { return c.*func(std::forward<P...>(p...)); }, (R(*)(               C &, P...))nullptr); }
template<class C, class R, class... P> Function BindFunction(R (C::*func)(P...) const         ) { return BindFunctionResult([func](const          C & c, P... p) { return c.*func(std::forward<P...>(p...)); }, (R(*)(const          C &, P...))nullptr); }
template<class C, class R, class... P> Function BindFunction(R (C::*func)(P...)       volatile) { return BindFunctionResult([func](      volatile C & c, P... p) { return c.*func(std::forward<P...>(p...)); }, (R(*)(      volatile C &, P...))nullptr); }
template<class C, class R, class... P> Function BindFunction(R (C::*func)(P...) const volatile) { return BindFunctionResult([func](const volatile C & c, P... p) { return c.*func(std::forward<P...>(p...)); }, (R(*)(const volatile C &, P...))nullptr); }

template<class F> Function BindFunction(F func, std::string name) { auto f = BindFunction(func); f.name = move(name); return f; }

template<class... P> class Event
{
    std::shared_ptr<const Program> program;
public:
    Event(std::shared_ptr<const Program> program) : program(move(program)) { assert(this->program->GetArgumentCount() == sizeof...(P)); }

    void operator() (P... p) const { program->Execute({&p...}); }
};

#include <iostream>

float add(float a, float b) { return a+b; }
float mul(float a, float b) { return a*b; }
void print(float x) { std::cout << x << std::endl; }

int main()
{
    auto prog = std::make_shared<const Program>(2, std::vector<Line>{
        {BindFunction(mul,"mul"), {-1,-1}},
        {BindFunction(add,"add"), {-2, 0}},
        {BindFunction(print,"print"), {1}},
        {BindFunction(add,"add"), {0, 1}},
    });
    std::cout << *prog << std::endl;
    
    const auto ev = Event<float,float>(prog);

    ev(3,2);

    return 0;
}
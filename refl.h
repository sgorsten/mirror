#ifndef REFL_H
#define REFL_H

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
std::ostream & operator << (std::ostream & out, const VarType & vt);

struct Function
{
    std::function<std::shared_ptr<void>(void **)> impl;
    std::vector<VarType>                params;
    VarType                             result;
    std::string                         name;

    std::shared_ptr<void>               Invoke(void * args[]) const { return impl(args); }

    template<class F> static Function   Bind(F func, std::string name) { auto f = Bind(func); f.name = move(name); return f; }
private:
    // Bind accepts pointers to free functions and member functions, deduces the call signature, and passes the results to BindWithSignature
    template<         class R, class... P> static Function Bind(R (   *func)(P...)               ) { return BindWithSignature( func,                                                                              (R(*)(                    P...))nullptr); }
    template<class C, class R, class... P> static Function Bind(R (C::*func)(P...)               ) { return BindWithSignature([func](               C & c, P... p) { return c.*func(std::forward<P...>(p...)); }, (R(*)(               C &, P...))nullptr); }
    template<class C, class R, class... P> static Function Bind(R (C::*func)(P...) const         ) { return BindWithSignature([func](const          C & c, P... p) { return c.*func(std::forward<P...>(p...)); }, (R(*)(const          C &, P...))nullptr); }
    template<class C, class R, class... P> static Function Bind(R (C::*func)(P...)       volatile) { return BindWithSignature([func](      volatile C & c, P... p) { return c.*func(std::forward<P...>(p...)); }, (R(*)(      volatile C &, P...))nullptr); }
    template<class C, class R, class... P> static Function Bind(R (C::*func)(P...) const volatile) { return BindWithSignature([func](const volatile C & c, P... p) { return c.*func(std::forward<P...>(p...)); }, (R(*)(const volatile C &, P...))nullptr); }

    // BindWithSignature accepts a function option and a call signature, and creates a Function instance, with both metadata and an implementation which invokes CallWithArgs
    template<class F, class R, class... P> static Function BindWithSignature(F func, R   (*)(P...)) { Function f; f.result = VarType::Get<R   >(); f.InitParameterList((std::tuple<P...> *)nullptr); f.impl = [func](void * args[]) { return std::make_shared<R>(CallWithArgs(func, args, (R(*)(P...))nullptr)); }; return f; }
    template<class F, class R, class... P> static Function BindWithSignature(F func, R & (*)(P...)) { Function f; f.result = VarType::Get<R & >(); f.InitParameterList((std::tuple<P...> *)nullptr); f.impl = [func](void * args[]) { return std::shared_ptr<void>(&CallWithArgs(func, args, (R & (*)(P...))nullptr)); }; return f; }
    template<class F, class R, class... P> static Function BindWithSignature(F func, R &&(*)(P...)) { Function f; f.result = VarType::Get<R &&>(); f.InitParameterList((std::tuple<P...> *)nullptr); f.impl = [func](void * args[]) { return std::shared_ptr<void>(&CallWithArgs(func, args, (R &&(*)(P...))nullptr)); }; return f; }
    template<class F,          class... P> static Function BindWithSignature(F func, void(*)(P...)) { Function f; f.result = VarType::Get<void>(); f.InitParameterList((std::tuple<P...> *)nullptr); f.impl = [func](void * args[]) { CallWithArgs(func, args, (void(*)(P...))nullptr); return std::shared_ptr<void>(); }; return f; }

    // InitParameterList fills out the function parameter list from a list of parameter types
    template<class P, class... PS> void InitParameterList(std::tuple<P,PS...> *) { params.push_back(VarType::Get<P>()); InitParameterList((std::tuple<PS...> *)nullptr); }
                                   void InitParameterList(std::tuple<       > *) {}

    // CallWithArgs invokes a function object with a list of arguments provided as an array of void pointers. It calls PassByArg to convert each argument pointer to the correct parameter type.
    template<class F, class R                                    > static R CallWithArgs(F func, void * args[], R(*)(       )) { return func(                                                                                  ); }
    template<class F, class R, class A                           > static R CallWithArgs(F func, void * args[], R(*)(A      )) { return func(PassArg<A>(args[0])                                                               ); }
    template<class F, class R, class A, class B                  > static R CallWithArgs(F func, void * args[], R(*)(A,B    )) { return func(PassArg<A>(args[0]), PassArg<B>(args[1])                                          ); }
    template<class F, class R, class A, class B, class C         > static R CallWithArgs(F func, void * args[], R(*)(A,B,C  )) { return func(PassArg<A>(args[0]), PassArg<B>(args[1]), PassArg<C>(args[2])                     ); }
    template<class F, class R, class A, class B, class C, class D> static R CallWithArgs(F func, void * args[], R(*)(A,B,C,D)) { return func(PassArg<A>(args[0]), PassArg<B>(args[1]), PassArg<C>(args[2]), PassArg<D>(args[3])); }

    // PassArg takes a void pointer and casts it to an appropriate type to be passed to a function
    template<class T> static T    PassArg(void * addr                    ) { return PassArg(addr, (std::tuple<T> *)nullptr); }
    template<class T> static T &  PassArg(void * addr, std::tuple<T & > *) { return             *reinterpret_cast<T *>(addr);   } // For lvalue reference types, just pass a ref to the object
    template<class T> static T && PassArg(void * addr, std::tuple<T &&> *) { return   std::move(*reinterpret_cast<T *>(addr));  } // For rvalue reference types, pass an rvalue ref to the object
    template<class T> static T    PassArg(void * addr, std::tuple<T   > *) { return T(std::move(*reinterpret_cast<T *>(addr))); } // For value types, move-construct a new value from the original
};
std::ostream & operator << (std::ostream & out, const Function & f);

struct Line
{
    Function func;
    std::vector<int> args;
};
std::ostream & operator << (std::ostream & out, const Line & line);

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

    void Execute(const std::vector<void *> & args) const
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
    }

    friend std::ostream & operator << (std::ostream & out, const Program & prog);
};

template<class... P> class Event
{
    std::shared_ptr<const Program> program;
public:
    Event(std::shared_ptr<const Program> program) : program(move(program)) { assert(this->program->GetArgumentCount() == sizeof...(P)); }

    void operator() (P... p) const { program->Execute({&p...}); }
};

#endif
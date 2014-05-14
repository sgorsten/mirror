#ifndef REFL_H
#define REFL_H

#include <cassert>
#include <vector>
#include <functional>
#include <memory>
#include <typeindex>
#include <string>
#include <ostream>
#include <map>

struct Type;

struct VarType
{
    enum                                Indirection                 { None, LValueRef, RValueRef };
    const Type *                        type;
    bool                                isConst;
    bool                                isVolatile;
    Indirection                         indirection;
};
std::ostream & operator << (std::ostream & out, const VarType & vt);

struct Type
{
    enum Kind                           { None, Other, Array, Pointer, FieldPointer, FunctionPointer, MethodPointer, Function };

    std::type_index                     type;
    size_t                              size;
    Kind                                kind;
    std::type_index                     elemType,objType;
    std::vector<VarType>                paramTypes;
    VarType                             returnType;

                                        Type()                      : type(typeid(void)), size(), kind(None), elemType(type), objType(type) {}
};

class TypeLibrary
{
    template<class T> struct                    SizeOf                      { enum { VALUE = sizeof(T) }; };
    template<> struct                           SizeOf<void>                { enum { VALUE = 0 }; }; // Void does not occupy space (but void pointers do!)
    template<class R, class... P> struct        SizeOf<R(P...)>             { enum { VALUE = 0 }; }; // Functions do not occupy space (but function pointers do!)
    std::map<std::type_index, Type>             types;

    template<class T                     > void InitType(Type & type, T      *                      ) { type.kind = Type::Other;                                                                                                        }
    template<class E, int N              > void InitType(Type & type, E     (*)[N]                  ) { type.kind = Type::Array;           type.elemType = &DeduceType<E>();                                                            }
    template<class E                     > void InitType(Type & type, E     **                      ) { type.kind = Type::Pointer;         type.elemType = &DeduceType<E>();                                                            }
    template<class E, class C            > void InitType(Type & type, E  C::**                      ) { type.kind = Type::FieldPointer;    type.elemType = &DeduceType<E>();                           type.objType = &DeduceType<C>(); }
    template<class R,          class... P> void InitType(Type & type, R    (**)(P...)               ) { type.kind = Type::FunctionPointer; type.elemType = &DeduceType<R(                    P...)>();                                  }
    template<class R, class C, class... P> void InitType(Type & type, R (C::**)(P...)               ) { type.kind = Type::MethodPointer;   type.elemType = &DeduceType<R(const          C &, P...)>(); type.objType = &DeduceType<C>(); }
    template<class R, class C, class... P> void InitType(Type & type, R (C::**)(P...) const         ) { type.kind = Type::MethodPointer;   type.elemType = &DeduceType<R(      volatile C &, P...)>(); type.objType = &DeduceType<C>(); }
    template<class R, class C, class... P> void InitType(Type & type, R (C::**)(P...)       volatile) { type.kind = Type::MethodPointer;   type.elemType = &DeduceType<R(const volatile C &, P...)>(); type.objType = &DeduceType<C>(); }
    template<class R, class C, class... P> void InitType(Type & type, R (C::**)(P...) const volatile) { type.kind = Type::MethodPointer;   type.elemType = &DeduceType<R(               C &, P...)>(); type.objType = &DeduceType<C>(); }
    template<class R,          class... P> void InitType(Type & type, R     (*)(P...)               ) { type.kind = Type::Function; type.returnType = DeduceVarType<R>(); InitParameterList(type, (std::tuple<P...> *)nullptr); }
    template<         class T, class... P> void InitParameterList(Type & type, std::tuple<T,P...> * ) { type.paramTypes.push_back(DeduceVarType<T>()); InitParameterList(type, (std::tuple<P...> *)nullptr); }
                                           void InitParameterList(Type & type, std::tuple<      > * ) {}
public:
    template<class T> const Type &      DeduceType()                { auto & type = types[typeid(T)]; if(type.kind == Type::None) { type.type = typeid(T); type.size = SizeOf<T>::VALUE; InitType(type, (T*)nullptr); } return type; }
    template<class T> VarType           DeduceVarType()             { typedef std::remove_reference_t<T> U; return { &DeduceType<std::remove_cv_t<U>>(), std::is_const<U>::value, std::is_volatile<U>::value, std::is_lvalue_reference<T>::value ? VarType::LValueRef : std::is_lvalue_reference<T>::value ? VarType::RValueRef : VarType::None }; }
};

struct Function
{
    std::function<std::shared_ptr<void>(void **)> impl;
    const Type *                        type;
    std::string                         name;

    std::shared_ptr<void>               Invoke(void * args[]) const { return impl(args); }

    template<class F> static Function   Bind(TypeLibrary & types, F func, std::string name) { auto f = Bind(types, func); f.name = move(name); return f; }
private:
    // Bind accepts pointers to free functions and member functions, deduces the call signature, and passes the results to BindWithSignature
    template<         class R, class... P> static Function Bind(TypeLibrary & types, R (   *func)(P...)               ) { return BindWithSignature(types,  func,                                                                             (R(*)(                    P...))nullptr); }
    template<class C, class R, class... P> static Function Bind(TypeLibrary & types, R (C::*func)(P...)               ) { return BindWithSignature(types, [func](               C & c, P... p) { return (c.*func)(std::forward<P>(p)...); }, (R(*)(               C &, P...))nullptr); }
    template<class C, class R, class... P> static Function Bind(TypeLibrary & types, R (C::*func)(P...) const         ) { return BindWithSignature(types, [func](const          C & c, P... p) { return (c.*func)(std::forward<P>(p)...); }, (R(*)(const          C &, P...))nullptr); }
    template<class C, class R, class... P> static Function Bind(TypeLibrary & types, R (C::*func)(P...)       volatile) { return BindWithSignature(types, [func](      volatile C & c, P... p) { return (c.*func)(std::forward<P>(p)...); }, (R(*)(      volatile C &, P...))nullptr); }
    template<class C, class R, class... P> static Function Bind(TypeLibrary & types, R (C::*func)(P...) const volatile) { return BindWithSignature(types, [func](const volatile C & c, P... p) { return (c.*func)(std::forward<P>(p)...); }, (R(*)(const volatile C &, P...))nullptr); }

    // BindWithSignature accepts a function option and a call signature, and creates a Function instance, with both metadata and an implementation which invokes CallWithArgs
    template<class F, class R, class... P> static Function BindWithSignature(TypeLibrary & types, F func, R   (*)(P...)) { Function f; f.type = &types.DeduceType<R   (P...)>(); f.impl = [func](void * args[]) { return std::make_shared<R>(CallWithArgs(func, args, (R(*)(P...))nullptr)); }; return f; }
    template<class F, class R, class... P> static Function BindWithSignature(TypeLibrary & types, F func, R & (*)(P...)) { Function f; f.type = &types.DeduceType<R & (P...)>(); f.impl = [func](void * args[]) { return std::shared_ptr<void>(&CallWithArgs(func, args, (R & (*)(P...))nullptr)); }; return f; }
    template<class F, class R, class... P> static Function BindWithSignature(TypeLibrary & types, F func, R &&(*)(P...)) { Function f; f.type = &types.DeduceType<R &&(P...)>(); f.impl = [func](void * args[]) { return std::shared_ptr<void>(&CallWithArgs(func, args, (R &&(*)(P...))nullptr)); }; return f; }
    template<class F,          class... P> static Function BindWithSignature(TypeLibrary & types, F func, void(*)(P...)) { Function f; f.type = &types.DeduceType<void(P...)>(); f.impl = [func](void * args[]) { CallWithArgs(func, args, (void(*)(P...))nullptr); return std::shared_ptr<void>(); }; return f; }

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
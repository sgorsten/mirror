// mirror/event.h
#ifndef MIRROR_EVENT_H
#define MIRROR_EVENT_H

#include "refl.h"

class NodeType
{
    friend class Program;
    struct Impl; std::shared_ptr<const Impl> impl;
public:
    struct Pin { std::string label; VarType type; };

    const std::string &         GetUniqueId() const;
    const std::string &         GetLabel() const;
    const std::vector<Pin> &    GetInputs() const;
    const std::vector<Pin> &    GetOutputs() const;
    bool                        HasInFlow() const;
    bool                        HasOutFlow() const;

    static NodeType             MakeEventNode(std::string name, std::vector<VarType> params);
    static NodeType             MakeFunctionNode(const Function & function);
    static NodeType             MakeSplitNode(const Type & type);
    static NodeType             MakeBuildNode(const Type & type);
};

class Program
{
    struct Impl; std::shared_ptr<const Impl> impl; 
public:
    struct Line { NodeType type; std::vector<size_t> inputs, outputs; };

    static Program Load(std::vector<std::shared_ptr<void>> constants, std::vector<Line> lines);

    void Invoke(void * args[], size_t argCount) const;
};

template<class F> class Event;
template<class... P> class Event<void(P...)>
{
    Program program;
public:
    Event() {}
    Event(Program program) : program(program) {}

    void operator()(P... p) const
    {
        void * args[] = {&p...};
        program.Invoke(args, sizeof...(P));
    }
};

#endif
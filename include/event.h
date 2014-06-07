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

    static NodeType             MakeEventNode(std::string name);
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

    void operator()() const;
};

#endif
// mirror/event.h
#ifndef MIRROR_EVENT_H
#define MIRROR_EVENT_H

#include "refl.h"

struct NodeType
{
    struct Pin { std::string label; VarType type; };
    std::string uniqueId, label;
    std::vector<Pin> inputs, outputs;
    bool hasInFlow;
    bool hasOutFlow;
    std::function<std::vector<std::shared_ptr<void>>(void **)> eval;

    const std::string & GetUniqueId() const { return uniqueId; }
    const std::string & GetLabel() const { return label; }

    static NodeType MakeEventNode(std::string name);
    static NodeType MakeFunctionNode(const Function & function);
    static NodeType MakeSplitNode(const Type & type);
    static NodeType MakeBuildNode(const Type & type);
};


struct Program
{
    struct Call
    {
        const NodeType * nodeType;
        std::vector<size_t> inputSlotIndices;
        std::vector<size_t> outputSlotIndices;
    };

    std::vector<std::shared_ptr<void>>  constants;          // Program constants, which occupy the first set of temporary slots
    size_t                              numTemporarySlots;  // Total number of temporary slots needed
    std::vector<Call>                   calls;              // List of calls to be made

    void                                Execute() const;    // Execute the program
};

class Event
{
    std::shared_ptr<const Program> program;
public:
    Event() : program() {}
    Event(std::shared_ptr<const Program> program) : program(program) {}

    void operator()() const 
    { 
        if(!program) return;
        program->Execute(); 
    }
};

#endif
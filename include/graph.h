// mirror/graph.h
// Provides a simple computation graph structure, which can be used to express event handlers as data
#ifndef MIRROR_GRAPH_H
#define MIRROR_GRAPH_H

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
    size_t GetInputCount() const { return inputs.size(); }
    size_t GetOutputCount() const { return outputs.size(); }
    VarType GetInputType(size_t index) const { return inputs[index].type; }
    VarType GetOutputType(size_t index) const { return outputs[index].type; }
    std::string GetInputLabel(size_t index) const { return inputs[index].label; }
    std::string GetOutputLabel(size_t index) const { return outputs[index].label; }
    std::vector<std::shared_ptr<void>> Evaluate(void * inputs[]) const { return eval(inputs); }

    static NodeType MakeEventNode(std::string name);
    static NodeType MakeFunctionNode(const Function & function);
    static NodeType MakeSplitNode(const Type & type);
    static NodeType MakeBuildNode(const Type & type);
};

struct Node
{
    struct Wire
    {
        int             nodeIndex, pinIndex;
        std::string     immediate;
    };

    const NodeType *    nodeType;                                   // The type of this node
    std::vector<Wire>   inputs;                                     // Wires which carry data from other nodes' outputs to this node's inputs
    int                 flowOutputIndex;                            // Flow control wire which passes execution from this node to another node after it is run

    int                 x,y;                                        // A set of coordinates, provided for visualization/editing convenience. Has no effect on execution, but will be serialized to/from JSON.
    bool                selected;                                   // A selection flag, provided for visualization/editing convenience. Has no effect on execution, and will not be serialized to/from JSON.

                        Node()                                      : nodeType(), flowOutputIndex(-1), x(), y(), selected() {}
                        Node(const NodeType & type, int x, int y)   : nodeType(&type), inputs(type.GetInputCount(),{-1,-1}), flowOutputIndex(-1), x(x), y(y), selected() {}
};

struct Program
{
    struct Call
    { 
        const NodeType *    type;
        std::vector<size_t> inputSlotIndices;
        std::vector<size_t> outputSlotIndices;
    };

    std::vector<std::shared_ptr<void>>  constants;          // Program constants, which occupy the first set of temporary slots
    size_t                              numTemporarySlots;  // Total number of temporary slots needed
    std::vector<Call>                   calls;              // List of calls to be made

    void Execute() const;                                       // Execute the program
};

Program Compile(const std::vector<Node> & nodes, int startIndex);

class JsonValue;
JsonValue SaveGraph(const std::vector<Node> & nodes);
std::vector<Node> LoadGraph(const std::vector<NodeType> & nodeTypes, const JsonValue & jsonGraph);

#endif
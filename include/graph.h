// mirror/graph.h
// Provides a simple computation graph structure, which can be used to express event handlers as data
#ifndef MIRROR_GRAPH_H
#define MIRROR_GRAPH_H

#include "event.h"

struct Node
{
    struct Wire
    {
        int             nodeIndex, pinIndex;
        std::string     immediate;
    };

    NodeType            type;                                       // The type of this node
    std::vector<Wire>   inputs;                                     // Wires which carry data from other nodes' outputs to this node's inputs
    int                 flowOutputIndex;                            // Flow control wire which passes execution from this node to another node after it is run

    int                 x,y;                                        // A set of coordinates, provided for visualization/editing convenience. Has no effect on execution, but will be serialized to/from JSON.
    bool                selected;                                   // A selection flag, provided for visualization/editing convenience. Has no effect on execution, and will not be serialized to/from JSON.

                        Node()                                      : type(), flowOutputIndex(-1), x(), y(), selected() {}
                        Node(const NodeType & type, int x, int y)   : type(type), inputs(type.GetInputs().size(), {-1,-1}), flowOutputIndex(-1), x(x), y(y), selected() {}
};

Program Compile(const std::vector<Node> & nodes, int startIndex);

class JsonValue;
JsonValue SaveGraph(const std::vector<Node> & nodes);
std::vector<Node> LoadGraph(const std::vector<NodeType> & nodeTypes, const JsonValue & jsonGraph);

#endif
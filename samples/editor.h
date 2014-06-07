#ifndef EDITOR_H
#define EDITOR_H

#include "graph.h"

struct Feature
{
    enum                                Type                                                { None, Body, Input, Output, FlowInput, FlowOutput };
    int2                                coord;                                              // Coordinates of the mouse when it was over this feature
    Type                                type;
    Node *                              node;
    size_t                              pin;

    bool                                IsFlowPin() const                                   { return type == FlowInput || type == FlowOutput; }
    bool                                IsDataPin() const                                   { return type == Input || type == Output; }
    VarType                             GetPinType() const                                  { assert(IsDataPin()); return type == Input ? node->GetInputType(pin) : node->GetOutputType(pin); }
    Rect                                GetPinRect() const;
};

struct GraphEditor
{
    std::vector<NodeType> nodeTypes;
    std::vector<Node> nodes;  

    enum Mode { None, Dragging, NewNodePopup };
    Mode mode;              // What mode the editor is in
    Feature mouseover;      // The current graph feature that the mouse is over
    Feature clicked;        // (Dragging mode) The last graph feature that the mouse clicked
    int2 menuPos;           // (NewNodePopup mode) If a context menu has been opened, the location of the mouse when it was opened    

    GraphEditor() : mode(None), mouseover({}), clicked({}) {}

    bool IsAssignable(const VarType & source, const VarType & target) const;
    bool IsCompatible(const Feature & a, const Feature & b) const;
    void ConnectPins(Feature a, Feature b);

    void RecomputeNode(Node & n);

    void ExecuteNode(Node & n);

    Feature GetFeature(const int2 & coord);

    void DeleteNode(int index);
    void DeleteSelection();

    void Draw() const;
};

#endif
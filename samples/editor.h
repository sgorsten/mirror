#ifndef EDITOR_H
#define EDITOR_H

#include "graph.h"

int GetStringWidth12(const std::string & text);
int GetStringWidth18(const std::string & text);

struct NodeView
{
    const Node & node;
    NodeView(const Node & node) : node(node) {}

    const NodeType &                    GetNodeType() const                                 { return *node.nodeType; }
    const int2 &                        GetPosition() const                                 { return node.position; }

    int                                 GetInputCount() const                               { return GetNodeType().GetInputCount(); }
    int                                 GetOutputCount() const                              { return GetNodeType().GetOutputCount(); }
    VarType                             GetInputType(size_t index) const                    { return GetNodeType().GetInputType(index); }
    VarType                             GetOutputType(size_t index) const                   { return GetNodeType().GetOutputType(index); }

    int                                 GetPinSize() const                                  { return 16; }
    int                                 GetPinPadding() const                               { return 2; }
    int                                 GetColumnPadding() const                            { return 10; }
    int                                 GetLineSize() const                                 { return 22; }
    int                                 GetLinePadding() const                              { return 2; }

    std::string                         GetInputLabel(size_t index) const                   { return GetNodeType().GetInputLabel(index); }
    std::string                         GetOutputLabel(size_t index) const                  { return GetNodeType().GetOutputLabel(index); }

    bool                                HasInFlow() const                                   { return GetNodeType().hasInFlow; }
    bool                                HasOutFlow() const                                  { return GetNodeType().hasOutFlow; }
    int                                 GetFlowControlSize() const                          { return HasInFlow() || HasOutFlow() ? GetPinSpacing() : 0; }
    Rect                                GetFlowInputRect() const                            { return GetPinRect(GetPosition().x,GetPosition().y); }
    Rect                                GetFlowOutputRect() const                           { return GetPinRect(GetPosition().x+GetSizeX()-GetPinSize(),GetPosition().y); }

    int                                 GetPinSpacing() const                               { return GetPinSize() + GetPinPadding(); }
    int                                 GetLineSpacing() const                              { return GetLineSize() + GetLinePadding(); }
    int                                 GetPinColumnSize(int pins) const                    { return pins>0 ? (GetPinSize() + GetPinSpacing()*(pins-1)) : 0; }
    int                                 GetLineColumnSize(int pins) const                   { return pins>0 ? (GetLineSize() + GetLineSpacing()*(pins-1)) : 0; }
    int                                 GetInputColumnSize() const                          { return GetPinColumnSize(GetInputCount()); }
    int                                 GetContentsColumnSize() const                       { return GetLineColumnSize(1); }
    int                                 GetOutputColumnSize() const                         { return GetPinColumnSize(GetOutputCount()); }
    int                                 GetInnerSizeY() const                               { return std::max<int>({GetInputColumnSize(), GetContentsColumnSize(), GetOutputColumnSize()}); }
    int                                 GetSizeY() const                                    { return GetFlowControlSize() + GetInnerSizeY(); }

    int                                 GetInputColumnLabelWidth() const                    { int w=0; for(size_t i=0, n=GetInputCount(); i!=n; ++i) w = std::max(w, GetStringWidth12(GetInputLabel(i))); return w; }
    int                                 GetContentsColumnWidth() const                      { return GetStringWidth18(GetNodeType().GetLabel()); }
    int                                 GetOutputColumnLabelWidth() const                   { int w=0; for(size_t i=0, n=GetOutputCount(); i!=n; ++i) w = std::max(w, GetStringWidth12(GetOutputLabel(i))); return w; }
    int                                 GetSizeX() const                                    { return GetPinSpacing() * 2 + GetInputColumnLabelWidth() + GetContentsColumnWidth() + GetOutputColumnLabelWidth() + GetColumnPadding() * 2; }

    Rect                                GetNodeRect() const                                 { return {GetPosition(), GetPosition() + int2(GetSizeX(),GetSizeY())}; }
    Rect                                GetPinRect(int x, int y) const                      { return {int2(x,y), int2(x,y) + int2(GetPinSize(),GetPinSize())}; }
    Rect                                GetInputPinRect(size_t i) const                     { return GetPinRect(GetPosition().x, GetPosition().y + GetFlowControlSize() + (GetInnerSizeY()-GetInputColumnSize())/2 + (int)i*GetPinSpacing()); }
    Rect                                GetOutputPinRect(size_t i) const                    { return GetPinRect(GetPosition().x+GetSizeX()-GetPinSize(), GetPosition().y + GetFlowControlSize() + (GetInnerSizeY()-GetOutputColumnSize())/2 + (int)i*GetPinSpacing()); }
    Rect                                GetContentsRect() const                             { auto x0 = GetPosition().x+GetPinSize()+GetInputColumnLabelWidth()+GetColumnPadding(); return {int2(x0,GetPosition().y), int2(x0,GetPosition().y) + int2(GetContentsColumnWidth(),GetSizeY())}; }
};

struct Feature
{
    enum                                Type                                                { None, Body, Input, Output, FlowInput, FlowOutput };
    int2                                coord;                                              // Coordinates of the mouse when it was over this feature
    Type                                type;
    Node *                              node;
    size_t                              pin;

    bool                                IsFlowPin() const                                   { return type == FlowInput || type == FlowOutput; }
    bool                                IsDataPin() const                                   { return type == Input || type == Output; }
    VarType                             GetPinType() const                                  { assert(IsDataPin()); return type == Input ? node->nodeType->GetInputType(pin) : node->nodeType->GetOutputType(pin); }
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
#ifndef GRAPH_H
#define GRAPH_H

#include "refl.h"

#include <sstream>
#include <algorithm>

template<class T> struct vec2
{ 
    T       x,y; 
            vec2()                              : x(), y() {}
            vec2(T x, T y)                      : x(x), y(y) {}
    vec2    operator + (const vec2 & r) const   { return {x+r.x, y+r.y}; }
    vec2    operator - (const vec2 & r) const   { return {x-r.x, y-r.y}; }
    vec2    operator * (T r) const              { return {x*r, y*r}; }
    vec2    operator / (T r) const              { return {x/r, y/r}; }
    vec2 &  operator += (const vec2 & r)        { return *this = *this + r; }
    vec2 &  operator -= (const vec2 & r)        { return *this = *this - r; }
    vec2 &  operator *= (T r)                   { return *this = *this * r; }
    vec2 &  operator /= (T r)                   { return *this = *this / r; }
};
template<class T> T dot(const vec2<T> & a, const vec2<T> & b) { return a.x*b.x + a.y*b.y; }
template<class T> T mag2(const vec2<T> & a) { return dot(a,a); }
template<class T> T mag(const vec2<T> & a) { return std::sqrt(mag2(a)); }
template<class T> vec2<T> norm(const vec2<T> & a) { return a/mag(a); }
typedef vec2<int> int2;
typedef vec2<float> float2;

struct Rect 
{
    int2 b0,b1;
    int2 GetCenter() const { return (b0+b1)/2; }
    bool Contains(const int2 & p) const { return p.x >= b0.x && p.y >= b0.y && p.x < b1.x && p.y < b1.y; }
};

class ToString
{
    std::ostringstream ss;
public:
    operator std::string() const { return ss.str(); }
    template<class T> ToString & operator << (const T & val) { ss << val; return *this; }
};

int GetStringWidth12(const std::string & text);
int GetStringWidth18(const std::string & text);

void WriteValue(std::ostream & out, const Type & type, void * value);
std::string ToStr(const Type & type, void * value);

struct NodeType
{
    struct Pin { std::string label; VarType type; };
    std::string uniqueId, label;
    std::vector<Pin> inputs, outputs;
    bool isSequenced;
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

    static NodeType MakeFunctionNode(const Function & function)
    {
        NodeType n = {};
        n.uniqueId = ToString() << "func:" << function;
        n.label = function.GetName();
        for(size_t i=0; i<function.GetParamCount(); ++i) n.inputs.push_back({function.GetParamName(i), function.GetParamType(i)});
        if(function.GetReturnType().type->index != typeid(void)) n.outputs.push_back({"", function.GetReturnType()});
        n.isSequenced = true;
        n.eval = [&function](void ** inputs) { return std::vector<std::shared_ptr<void>>{function.Invoke(inputs)}; };
        return n;
    }

    static NodeType MakeSplitNode(const Type & type)
    {
        NodeType n = {};
        n.uniqueId = ToString() << "split:" << type;
        n.label = ToString() << "split " << type;
        n.inputs.push_back({"", {&type, false, false, VarType::LValueRef}});
        for(auto & f : type.fields) n.outputs.push_back({f.identifier, f.type});
        n.eval = [&type](void ** inputs) 
        {
            std::vector<std::shared_ptr<void>> outputs; 
            for(auto & field : type.fields) outputs.push_back(std::shared_ptr<void>(field.accessor(inputs[0]), [](void *){}));
            return outputs; 
        };
        return n;
    }

    static NodeType MakeBuildNode(const Type & type)
    {
        NodeType n = {};
        n.uniqueId = ToString() << "build:" << type;
        n.label = ToString() << "build " << type;
        for(auto & f : type.fields) n.inputs.push_back({f.identifier, f.type});
        n.outputs.push_back({"", {&type, false, false, VarType::None}});
        n.eval = [&type](void ** inputs) -> std::vector<std::shared_ptr<void>>
        {
            auto output = type.DefConstruct();
            for(auto & field : type.fields)
            {
                assert(field.type.indirection == VarType::None);
                field.type.type->CopyAssign(field.accessor(output.get()), *inputs++);
            }
            return {output};
        };
        return n;
    }
};

struct Node
{
    struct Wire { int nodeIndex, pinIndex; std::string immediate; };

    int2                                position;
    const NodeType *                    nodeType;
    std::vector<Wire>                   inputs;
    int                                 flowOutputIndex;

    std::vector<std::shared_ptr<void>>  outputValues;
    bool                                selected;

                                        Node()                                              : selected(), flowOutputIndex(-1) {}
                                        Node(const int2 & position, const NodeType * type)  : position(position), nodeType(type), inputs(GetInputCount(),{-1,-1}), flowOutputIndex(-1), outputValues(GetOutputCount()), selected() {}

    int                                 GetInputCount() const                               { return nodeType->GetInputCount(); }
    int                                 GetOutputCount() const                              { return nodeType->GetOutputCount(); }
    VarType                             GetInputType(size_t index) const                    { return nodeType->GetInputType(index); }
    VarType                             GetOutputType(size_t index) const                   { return nodeType->GetOutputType(index); }

    int                                 GetPinSize() const                                  { return 16; }
    int                                 GetPinPadding() const                               { return 2; }
    int                                 GetColumnPadding() const                            { return 10; }
    int                                 GetLineSize() const                                 { return 22; }
    int                                 GetLinePadding() const                              { return 2; }

    std::string                         GetInputLabel(size_t index) const                   { return nodeType->GetInputLabel(index); }
    std::string                         GetOutputLabel(size_t index) const                  { return nodeType->GetOutputLabel(index); }

    bool                                IsSequenced() const                                 { return nodeType->isSequenced; }
    int                                 GetFlowControlSize() const                          { return IsSequenced() ? GetPinSpacing() : 0; }
    Rect                                GetFlowInputRect() const                            { return GetPinRect(position.x,position.y); }
    Rect                                GetFlowOutputRect() const                           { return GetPinRect(position.x+GetSizeX()-GetPinSize(),position.y); }

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
    int                                 GetContentsColumnWidth() const                      { return GetStringWidth18(nodeType->GetLabel()); }
    int                                 GetOutputColumnLabelWidth() const                   { int w=0; for(size_t i=0, n=GetOutputCount(); i!=n; ++i) w = std::max(w, GetStringWidth12(GetOutputLabel(i))); return w; }
    int                                 GetSizeX() const                                    { return GetPinSpacing() * 2 + GetInputColumnLabelWidth() + GetContentsColumnWidth() + GetOutputColumnLabelWidth() + GetColumnPadding() * 2; }

    Rect                                GetNodeRect() const                                 { return {position, position + int2(GetSizeX(),GetSizeY())}; }
    Rect                                GetPinRect(int x, int y) const                      { return {int2(x,y), int2(x,y) + int2(GetPinSize(),GetPinSize())}; }
    Rect                                GetInputPinRect(size_t i) const                     { return GetPinRect(position.x, position.y + GetFlowControlSize() + (GetInnerSizeY()-GetInputColumnSize())/2 + (int)i*GetPinSpacing()); }
    Rect                                GetOutputPinRect(size_t i) const                    { return GetPinRect(position.x+GetSizeX()-GetPinSize(), position.y + GetFlowControlSize() + (GetInnerSizeY()-GetOutputColumnSize())/2 + (int)i*GetPinSpacing()); }
    Rect                                GetContentsRect() const                             { auto x0 = position.x+GetPinSize()+GetInputColumnLabelWidth()+GetColumnPadding(); return {int2(x0,position.y), int2(x0,position.y) + int2(GetContentsColumnWidth(),GetSizeY())}; }
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
    Feature GetFeature(const int2 & coord);

    void DeleteNode(int index);
    void DeleteSelection();

    void Draw() const;
};

#endif
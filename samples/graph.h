#ifndef GRAPH_H
#define GRAPH_H

#include "refl.h"

#include <sstream>
#include <algorithm>

struct Rect { int x0,y0,x1,y1; };

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
        NodeType n;
        n.uniqueId = ToString() << "func:" << function;
        n.label = function.GetName();
        for(size_t i=0; i<function.GetParamCount(); ++i) n.inputs.push_back({function.GetParamName(i), function.GetParamType(i)});
        n.outputs.push_back({"", function.GetReturnType()});
        n.eval = [&function](void ** inputs) { return std::vector<std::shared_ptr<void>>{function.Invoke(inputs)}; };
        return n;
    }

    static NodeType MakeSplitNode(const Type & type)
    {
        NodeType n;
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
        NodeType n;
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

    int                                 x,y;
    const NodeType *                    nodeType;
    std::vector<Wire>                   inputs;

    std::vector<std::shared_ptr<void>>  outputValues;
    bool                                selected;

                                        Node() : x(), y(), selected() {}
                                        Node(int x, int y, const NodeType * type)           : x(x), y(y), nodeType(type), inputs(GetInputCount(),{-1,-1}), outputValues(GetOutputCount()), selected() {}

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

    int                                 GetPinSpacing() const                               { return GetPinSize() + GetPinPadding(); }
    int                                 GetLineSpacing() const                              { return GetLineSize() + GetLinePadding(); }
    int                                 GetPinColumnSize(int pins) const                    { return pins>0 ? (GetPinSize() + GetPinSpacing()*(pins-1)) : 0; }
    int                                 GetLineColumnSize(int pins) const                   { return pins>0 ? (GetLineSize() + GetLineSpacing()*(pins-1)) : 0; }
    int                                 GetInputColumnSize() const                          { return GetPinColumnSize(GetInputCount()); }
    int                                 GetContentsColumnSize() const                       { return GetLineColumnSize(2); }
    int                                 GetOutputColumnSize() const                         { return GetPinColumnSize(GetOutputCount()); }
    int                                 GetSizeY() const                                    { return std::max<int>({GetInputColumnSize(), GetContentsColumnSize(), GetOutputColumnSize()}); }

    int                                 GetInputColumnLabelWidth() const                    { int w=0; for(size_t i=0, n=GetInputCount(); i!=n; ++i) w = std::max(w, GetStringWidth12(GetInputLabel(i))); return w; }
    int                                 GetContentsColumnWidth() const                      { return GetStringWidth18(nodeType->GetLabel()); }
    int                                 GetOutputColumnLabelWidth() const                   { int w=0; for(size_t i=0, n=GetOutputCount(); i!=n; ++i) w = std::max(w, GetStringWidth12(GetOutputLabel(i))); return w; }
    int                                 GetSizeX() const                                    { return GetPinSpacing() * 2 + GetInputColumnLabelWidth() + GetContentsColumnWidth() + GetOutputColumnLabelWidth() + GetColumnPadding() * 2; }

    Rect                                GetNodeRect() const                                 { return {x,y,x+GetSizeX(),y+GetSizeY()}; }
    Rect                                GetPinRect(int x, int y) const                      { return {x,y,x+GetPinSize(),y+GetPinSize()}; }
    Rect                                GetInputPinRect(size_t i) const                     { return GetPinRect(x, y + (GetSizeY()-GetInputColumnSize())/2 + (int)i*GetPinSpacing()); }
    Rect                                GetOutputPinRect(size_t i) const                    { return GetPinRect(x+GetSizeX()-GetPinSize(), y + (GetSizeY()-GetOutputColumnSize())/2 + (int)i*GetPinSpacing()); }
    Rect                                GetContentsRect() const                             { auto x0 = x+GetPinSize()+GetInputColumnLabelWidth()+GetColumnPadding(); return {x0, y, x0+GetContentsColumnWidth(), y+GetSizeY()}; }
};

struct Feature
{
    enum                                Type                                                { None, Body, Input, Output };
    Type                                type;       
    Node *                              node;
    size_t                              pin;

    bool                                IsPin() const                                       { return type == Input || type == Output; }
    VarType                             GetPinType() const                                  { assert(IsPin()); return type == Input ? node->GetInputType(pin) : node->GetOutputType(pin); }
    Rect                                GetPinRect() const                                  { assert(IsPin()); return type == Input ? node->GetInputPinRect(pin) : node->GetOutputPinRect(pin); }
};

struct GraphEditor
{
    std::vector<Node> nodes;
    bool clicked;
    int clickedX, clickedY;
    int lastX, lastY;

    Feature mouseOverFeature;
    Feature clickedFeature;

    std::vector<NodeType> nodeTypes;
    bool creatingNewNode;

    GraphEditor() : clicked(), mouseOverFeature({}), clickedFeature({}), creatingNewNode() {}

    void ConnectPins(Feature a, Feature b);

    void RecomputeNode(Node & n);
    Feature GetFeature(int x, int y);

    void DeleteNode(int index);
    void DeleteSelection();
};

#endif
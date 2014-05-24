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

struct INodeType
{
    virtual void WriteLabel(std::ostream & out) const = 0;
    virtual size_t GetInputCount() const = 0;
    virtual size_t GetOutputCount() const = 0;
    virtual VarType GetInputType(size_t index) const = 0;
    virtual VarType GetOutputType(size_t index) const = 0;
    virtual std::vector<std::shared_ptr<void>> Evaluate(void * inputs[]) const = 0;
};

class FunctionNodeType : public INodeType
{
    const Function &                    function;
public:
                                        FunctionNodeType(const Function & function)         : function(function) {}

    void                                WriteLabel(std::ostream & out) const override       { out << function.GetName(); }
    size_t                              GetInputCount() const override                      { return function.GetParamTypes().size(); }
    size_t                              GetOutputCount() const override                     { return 1; }
    VarType                             GetInputType(size_t index) const override           { return function.GetParamTypes()[index]; }
    VarType                             GetOutputType(size_t index) const override          { return function.GetReturnType(); }
    std::vector<std::shared_ptr<void>>  Evaluate(void * inputs[]) const override            { return {function.Invoke(inputs)}; }
};

class VariableNodeType : public INodeType
{
    VarType                             type;
public:
                                        VariableNodeType(const VarType & type)              : type(type) {}

    void                                WriteLabel(std::ostream & out) const override       {}
    size_t                              GetInputCount() const override                      { return 0; }
    size_t                              GetOutputCount() const override                     { return 1; }
    VarType                             GetInputType(size_t index) const override           { assert(false); return {}; }
    VarType                             GetOutputType(size_t index) const override          { assert(index == 0); return type; }
    std::vector<std::shared_ptr<void>>  Evaluate(void * inputs[]) const override            { return {nullptr}; }
};

struct Node
{
    int                                 x,y;
    std::shared_ptr<const INodeType>    nodeType;
    std::vector<int>                    inputs;
    std::vector<std::shared_ptr<void>>  outputValues;

                                        Node() : x(), y() {}
                                        Node(int x, int y, const Function * function)       : x(x), y(y), nodeType(std::make_shared<FunctionNodeType>(*function)), inputs(GetInputCount(),-1), outputValues(nodeType->GetOutputCount()) {}
    template<class T>                   Node(int x, int y, TypeLibrary & types, T && value) : x(x), y(y), nodeType(std::make_shared<VariableNodeType>(types.DeduceVarType<T>())), outputValues({std::make_shared<T>(std::move(value))}) {}

    int                                 GetInputCount() const                               { return nodeType->GetInputCount(); }
    int                                 GetOutputCount() const                              { return nodeType->GetOutputCount(); }
    VarType                             GetInputType(size_t index) const                    { return nodeType->GetInputType(index); }
    VarType                             GetOutputType(size_t index) const                   { return nodeType->GetOutputType(index); }

    int                                 GetPinSize() const                                  { return 16; }
    int                                 GetPinPadding() const                               { return 2; }
    int                                 GetColumnPadding() const                            { return 10; }
    int                                 GetLineSize() const                                 { return 22; }
    int                                 GetLinePadding() const                              { return 2; }

    int                                 GetPinSpacing() const                               { return GetPinSize() + GetPinPadding(); }
    int                                 GetLineSpacing() const                              { return GetLineSize() + GetLinePadding(); }
    int                                 GetPinColumnSize(int pins) const                    { return pins>0 ? (GetPinSize() + GetPinSpacing()*(pins-1)) : 0; }
    int                                 GetLineColumnSize(int pins) const                   { return pins>0 ? (GetLineSize() + GetLineSpacing()*(pins-1)) : 0; }
    int                                 GetInputColumnSize() const                          { return GetPinColumnSize(GetInputCount()); }
    int                                 GetContentsColumnSize() const                       { return GetLineColumnSize(2); }
    int                                 GetOutputColumnSize() const                         { return GetPinColumnSize(GetOutputCount()); }
    int                                 GetSizeY() const                                    { return std::max<int>({GetInputColumnSize(), GetContentsColumnSize(), GetOutputColumnSize()}); }

    int                                 GetInputColumnLabelWidth() const                    { int w=0; for(size_t i=0, n=GetInputCount(); i!=n; ++i) w = std::max(w, GetStringWidth12(ToString() << GetInputType(i))); return w; }
    int                                 GetContentsColumnWidth() const                      { std::ostringstream ss; nodeType->WriteLabel(ss); return GetStringWidth18(ss.str()); }
    int                                 GetOutputColumnLabelWidth() const                   { int w=0; for(size_t i=0, n=GetOutputCount(); i!=n; ++i) w = std::max(w, GetStringWidth12(ToString() << ToStr(*GetOutputType(i).type, outputValues[i].get()) << " : " << GetOutputType(i))); return w; }
    int                                 GetSizeX() const                                    { return GetPinSpacing() * 2 + GetInputColumnLabelWidth() + GetContentsColumnWidth() + GetOutputColumnLabelWidth() + GetColumnPadding() * 2; }

    Rect                                GetNodeRect() const                                 { return {x,y,x+GetSizeX(),y+GetSizeY()}; }
    Rect                                GetPinRect(int x, int y) const                      { return {x,y,x+GetPinSize(),y+GetPinSize()}; }
    Rect                                GetInputPinRect(size_t i) const                     { return GetPinRect(x, y + (GetSizeY()-GetInputColumnSize())/2 + i*GetPinSpacing()); }
    Rect                                GetOutputPinRect(size_t i) const                    { return GetPinRect(x+GetSizeX()-GetPinSize(), y + (GetSizeY()-GetOutputColumnSize())/2 + i*GetPinSpacing()); }
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
    int lastX, lastY;

    Feature mouseOverFeature;
    Feature clickedFeature;

    GraphEditor() : mouseOverFeature(Feature{}), clickedFeature({}) {}

    void ConnectPins(Feature a, Feature b);

    void RecomputeNode(Node & n);
    Feature GetFeature(int x, int y);
};

#endif
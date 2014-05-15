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

struct Node
{
    int                     x,y;
    const Function *        function; // If non-null, this is a function node
    std::vector<int>        inputs;
    VarType                 type;
    std::shared_ptr<void>   value;

                            Node() : x(), y(), function() {}
                            Node(int x, int y, const Function * function)       : x(x), y(y), function(function), inputs(GetInputCount(),-1), type(function->GetReturnType()) {}
    template<class T>       Node(int x, int y, TypeLibrary & types, T && value) : x(x), y(y), function(), type(types.DeduceVarType<T>()), value(std::make_shared<T>(std::move(value))) {}

    int                     GetInputCount() const                               { return function ? (int)function->GetParamTypes().size() : 0; }
    int                     GetOutputCount() const                              { return 1; }

    int                     GetPinSize() const                                  { return 16; }
    int                     GetPinPadding() const                               { return 2; }
    int                     GetColumnPadding() const                            { return 10; }
    int                     GetLineSize() const                                 { return 22; }
    int                     GetLinePadding() const                              { return 2; }

    int                     GetPinSpacing() const                               { return GetPinSize() + GetPinPadding(); }
    int                     GetLineSpacing() const                               { return GetLineSize() + GetLinePadding(); }
    int                     GetPinColumnSize(int pins) const                    { return pins>0 ? (GetPinSize() + GetPinSpacing()*(pins-1)) : 0; }
    int                     GetLineColumnSize(int pins) const                    { return pins>0 ? (GetLineSize() + GetLineSpacing()*(pins-1)) : 0; }
    int                     GetInputColumnSize() const                          { return GetPinColumnSize(GetInputCount()); }
    int                     GetContentsColumnSize() const                       { return GetLineColumnSize(function ? 2 : 1); }
    int                     GetOutputColumnSize() const                         { return GetPinColumnSize(GetOutputCount()); }
    int                     GetSizeY() const                                    { return std::max<int>({GetInputColumnSize(), GetContentsColumnSize(), GetOutputColumnSize()}); }

    int                     GetInputColumnLabelWidth() const                    { if(!function) return 0; int w=0; for(auto & p : function->GetParamTypes()) w = std::max(w, GetStringWidth12(ToString() << p)); return w; }
    int                     GetContentsColumnWidth() const                      { return std::max(function ? GetStringWidth18(function->GetName()) : 0, GetStringWidth18(ToStr(*type.type, value.get()))); }
    int                     GetOutputColumnLabelWidth() const                   { return GetStringWidth12(ToString() << type); }
    int                     GetSizeX() const                                    { return GetPinSpacing() * 2 + GetInputColumnLabelWidth() + GetContentsColumnWidth() + GetOutputColumnLabelWidth() + GetColumnPadding() * 2; }

    Rect                    GetNodeRect() const                                 { return {x,y,x+GetSizeX(),y+GetSizeY()}; }
    Rect                    GetPinRect(int x, int y) const                      { return {x,y,x+GetPinSize(),y+GetPinSize()}; }
    Rect                    GetInputPinRect(size_t i) const                     { return GetPinRect(x, y + (GetSizeY()-GetInputColumnSize())/2 + i*GetPinSpacing()); }
    Rect                    GetOutputPinRect(size_t i) const                    { return GetPinRect(x+GetSizeX()-GetPinSize(), y + (GetSizeY()-GetOutputColumnSize())/2 + i*GetPinSpacing()); }
    Rect                    GetContentsRect() const                             { auto x0 = x+GetPinSize()+GetInputColumnLabelWidth()+GetColumnPadding(); return {x0, y, x0+GetContentsColumnWidth(), y+GetSizeY()}; }
};

struct Feature
{
    enum                    Type                                                { None, Body, Input, Output };
    Type                    type;       
    Node *                  node;
    size_t                  pin;
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
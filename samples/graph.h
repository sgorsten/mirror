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

size_t GetStringWidth(const std::string & text);

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
                            Node(int x, int y, const Function * function) : x(x), y(y), function(function), inputs(GetInputCount(),-1), type(function->GetReturnType()) {}
    template<class T>       Node(int x, int y, TypeLibrary & types, T && value) : x(x), y(y), function(), type(types.DeduceVarType<T>()), value(std::make_shared<T>(std::move(value))) {}

    size_t                  GetInputCount() const { return function ? function->GetParamTypes().size() : 0; }
    size_t                  GetOutputCount() const { return 1; }

    size_t                  GetPinSize() const { return 22; }
    size_t                  GetPinPadding() const { return 2; }
    size_t                  GetColumnPadding() const { return 10; }

    size_t                  GetPinSpacing() const { return GetPinSize() + GetPinPadding(); }
    size_t                  GetPinColumnSize(size_t pins) const { return pins>0 ? (GetPinSize() + GetPinSpacing()*(pins-1)) : 0; }
    size_t                  GetInputColumnSize() const { return GetPinColumnSize(GetInputCount()); }
    size_t                  GetContentsColumnSize() const { return GetPinColumnSize(function ? 2 : 1); }
    size_t                  GetOutputColumnSize() const { return GetPinColumnSize(GetOutputCount()); }
    size_t                  GetSizeY() const { return std::max<size_t>({GetInputColumnSize(), GetContentsColumnSize(), GetOutputColumnSize()}); }

    size_t                  GetInputColumnLabelWidth() const { if(!function) return 0; size_t w=0; for(auto & p : function->GetParamTypes()) w = std::max(w, GetStringWidth(ToString() << p)); return w; }
    size_t                  GetContentsColumnWidth() const { return std::max(function ? GetStringWidth(function->GetName()) : 0, GetStringWidth(ToStr(*type.type, value.get()))); }
    size_t                  GetOutputColumnLabelWidth() const { return GetStringWidth(ToString() << type); }
    size_t                  GetSizeX() const { return GetPinSpacing() * 2 + GetInputColumnLabelWidth() + GetContentsColumnWidth() + GetOutputColumnLabelWidth() + GetColumnPadding() * 2; }

    Rect                    GetNodeRect() const { return {x,y,x+GetSizeX(),y+GetSizeY()}; }
    Rect                    GetPinRect(int x, int y) const { return {x,y,x+GetPinSize(),y+GetPinSize()}; }
    Rect                    GetInputPinRect(size_t i) const { return GetPinRect(x, y + (GetSizeY()-GetInputColumnSize())/2 + i*GetPinSpacing()); }
    Rect                    GetOutputPinRect(size_t i) const { return GetPinRect(x+GetSizeX()-GetPinSize(), y + (GetSizeY()-GetOutputColumnSize())/2 + i*GetPinSpacing()); }
    Rect                    GetContentsRect() const { auto x0 = x+GetPinSize()+GetInputColumnLabelWidth()+GetColumnPadding(); return {x0, y, x0+GetContentsColumnWidth(), y+GetSizeY()}; }
};

struct Feature
{
    enum                    Type        { None, Body, Input, Output };
    Type                    type;       
    Node *                  node;
    size_t                  pin;

                            Feature()   : type(None), node(), pin() {}
};

struct GraphEditor
{
    std::vector<Node> nodes;
    int lastX, lastY;

    Feature mouseOverFeature;
    Feature clickedFeature;

    GraphEditor() {}

    void ConnectPins(Feature a, Feature b);

    void RecomputeNode(Node & n);
    void SetMousePosition(int x, int y);
};

#endif
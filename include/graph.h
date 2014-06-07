// mirror/graph.h
// Provides a simple computation graph structure, which can be used to express event handlers as data
#ifndef MIRROR_GRAPH_H
#define MIRROR_GRAPH_H


#include "refl.h"
#include "json.h"

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

void WriteValue(std::ostream & out, const Type & type, void * value);
std::string ToStr(const Type & type, void * value);

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

    static NodeType MakeEventNode(std::string name)
    {
        NodeType n = {};
        n.uniqueId = "event:"+name;
        n.label = "On "+name;
        n.hasOutFlow = true;
        n.eval = [](void ** inputs) { return std::vector<std::shared_ptr<void>>{}; };
        return n;        
    }

    static NodeType MakeFunctionNode(const Function & function)
    {
        NodeType n = {};
        n.uniqueId = ToString() << "func:" << function;
        n.label = function.GetName();
        for(size_t i=0; i<function.GetParamCount(); ++i) n.inputs.push_back({function.GetParamName(i), function.GetParamType(i)});
        if(function.GetReturnType().type->index != typeid(void)) n.outputs.push_back({"", function.GetReturnType()});
        n.hasInFlow = n.hasOutFlow = true;
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

    bool                                selected;

                                        Node()                                              : selected(), flowOutputIndex(-1) {}
                                        Node(const int2 & position, const NodeType * type)  : position(position), nodeType(type), inputs(type->GetInputCount(),{-1,-1}), flowOutputIndex(-1), selected() {}
};

void ExecuteEvent(const std::vector<Node> & nodes, int startIndex);

class JsonValue;
JsonValue SaveGraph(const std::vector<Node> & nodes);
std::vector<Node> LoadGraph(const std::vector<NodeType> & nodeTypes, const JsonValue & jsonGraph);

#endif
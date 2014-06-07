#include "editor.h"

void WriteValue(std::ostream & out, const Type & type, void * value)
{
    if(!value) return;
    switch(type.kind)
    {
    case Type::Fundamental:
        if(type.index == typeid(int)) out << *(int *)value;
        else if(type.index == typeid(float)) out << *(float *)value;
        else out << "???";
        break;
    default:
        out << "???";
        break;
    }
}

std::string ToStr(const Type & type, void * value)
{
    std::ostringstream ss;
    WriteValue(ss, type, value);
    return ss.str();
}

Rect Feature::GetPinRect() const
{
    switch(type)
    {
    case Input: return NodeView(*node).GetInputPinRect(pin);
    case Output: return NodeView(*node).GetOutputPinRect(pin);
    case FlowInput: return NodeView(*node).GetFlowInputRect();
    case FlowOutput: return NodeView(*node).GetFlowOutputRect();
    default: assert(false); return {};
    }
}

bool GraphEditor::IsAssignable(const VarType & source, const VarType & target) const
{
    // TODO: Const correctness, copyability, conversion to base, etc.
    return source.type == target.type;    
}

bool GraphEditor::IsCompatible(const Feature & a, const Feature & b) const
{
    switch(a.type)
    {
    case Feature::FlowInput: return b.type == Feature::FlowOutput;
    case Feature::FlowOutput: return b.type == Feature::FlowInput;
    case Feature::Input: return b.type == Feature::Output && IsAssignable(NodeView(*b.node).GetOutputType(b.pin), NodeView(*a.node).GetInputType(a.pin));
    case Feature::Output: return b.type == Feature::Input && IsAssignable(NodeView(*a.node).GetOutputType(a.pin), NodeView(*b.node).GetInputType(b.pin));
    default: return false;
    }
}

void GraphEditor::ConnectPins(Feature a, Feature b)
{
    if(b.type == Feature::FlowInput) std::swap(a, b);
    if(a.type == Feature::FlowInput && b.type == Feature::FlowOutput)
    {
        b.node->flowOutputIndex = a.node - nodes.data();
        return;
    }

    if(b.type == Feature::Input) std::swap(a, b);
    if(a.type == Feature::Input && b.type == Feature::Output)
    {
        auto paramType = NodeView(*a.node).GetInputType(a.pin);
        auto argType = NodeView(*b.node).GetOutputType(b.pin);
        if(paramType.type != argType.type) return;
        a.node->inputs[a.pin] = {b.node - nodes.data(), b.pin};
    }
}

Feature GraphEditor::GetFeature(const int2 & coord)
{
    for(auto & n : nodes)
    {
        NodeView nv(n);
        if(nv.GetNodeRect().Contains(coord))
        {
            if(nv.HasInFlow())
            {
                if(nv.GetFlowInputRect().Contains(coord))
                {
                    return {coord, Feature::FlowInput, &n, 0};
                }
            }

            if(nv.HasOutFlow())
            {
                if(nv.GetFlowOutputRect().Contains(coord))
                {
                    return {coord, Feature::FlowOutput, &n, 0};
                }
            }

            for(size_t i=0; i<nv.GetInputCount(); ++i)
            {
                if(nv.GetInputPinRect(i).Contains(coord))
                {
                    return {coord, Feature::Input, &n, i};
                }
            }

            for(size_t i=0; i<nv.GetOutputCount(); ++i)
            {
                if(nv.GetOutputPinRect(i).Contains(coord))
                {
                    return {coord, Feature::Output, &n, i};
                }
            }

            return {coord, Feature::Body, &n};
        }
    }

    return {coord, Feature::None};
}

void GraphEditor::DeleteNode(int index)
{
    for(auto & node : nodes)
    {
        if(node.flowOutputIndex > index) --node.flowOutputIndex;
        else if(node.flowOutputIndex == index) node.flowOutputIndex = -1;

        for(auto & input : node.inputs)
        {
            if(input.nodeIndex > index) --input.nodeIndex;
            else if(input.nodeIndex == index) input = {-1,-1};
        }
    }
    nodes.erase(begin(nodes) + index);

    clicked = mouseover = {};
}

void GraphEditor::DeleteSelection()
{
    for(int i = nodes.size()-1; i>=0; --i)
    {
        if(nodes[i].selected) DeleteNode(i);
    }
}
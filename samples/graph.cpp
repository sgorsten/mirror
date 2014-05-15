#include "graph.h"

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

void GraphEditor::ConnectPins(Feature a, Feature b)
{
    if(b.type == Feature::Input) std::swap(a, b);
    if(a.type == Feature::Input && b.type == Feature::Output)
    {
        auto paramType = a.node->function->GetParamTypes()[a.pin];
        auto argType = b.node->type;
        if(paramType.type != argType.type) return;

        a.node->inputs[a.pin] = b.node - nodes.data();
    }
}

void GraphEditor::RecomputeNode(Node & n)
{
    if(n.function)
    {
        void * args[8];
        for(size_t i=0; i<n.inputs.size(); ++i)
        {
            if(n.inputs[i] < 0) return;
            if(!nodes[n.inputs[i]].value) return;
            args[i] = nodes[n.inputs[i]].value.get();
        }
        n.value = n.function->Invoke(args);
        n.type = n.function->GetReturnType();
    }   
}

Feature GraphEditor::GetFeature(int x, int y)
{
    for(auto & n : nodes)
    {
        auto rect = n.GetNodeRect();
        if(x >= rect.x0 && x < rect.x1 && y >= rect.y0 && y < rect.y1)
        {
            mouseOverFeature.node = &n;

            for(size_t i=0; i<n.GetInputCount(); ++i)
            {
                rect = n.GetInputPinRect(i);
                if(x >= rect.x0 && x < rect.x1 && y >= rect.y0 && y < rect.y1)
                {
                    return {Feature::Input, &n, i};
                }
            }

            for(size_t i=0; i<n.GetOutputCount(); ++i)
            {
                rect = n.GetOutputPinRect(i);
                if(x >= rect.x0 && x < rect.x1 && y >= rect.y0 && y < rect.y1)
                {
                    return {Feature::Output, &n, i};
                }
            }

            return {Feature::Body, &n};
        }
    }

    return {Feature::None};
}
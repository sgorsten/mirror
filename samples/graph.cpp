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
        auto paramType = a.node->GetInputType(a.pin);
        auto argType = b.node->GetOutputType(b.pin);
        if(paramType.type != argType.type) return;
        a.node->inputs[a.pin] = {b.node - nodes.data(), b.pin};
    }
}

void GraphEditor::RecomputeNode(Node & n)
{  
    std::vector<std::shared_ptr<void>> immediates;
    void * args[8];
    for(size_t i=0; i<n.inputs.size(); ++i)
    {
        auto wire = n.inputs[i];
        if(wire.nodeIndex < 0)
        {
            if(wire.immediate.empty()) return;

            auto type = n.GetInputType(i);
            assert(type.indirection == VarType::None);
            if(type.type->index == typeid(int))
            {
                std::istringstream ss(wire.immediate);
                int value; if(!(ss >> value)) return;
                immediates.push_back(std::make_shared<int>(value));
            }
            else if(type.type->index == typeid(float))
            {
                std::istringstream ss(wire.immediate);
                float value; if(!(ss >> value)) return;
                immediates.push_back(std::make_shared<float>(value));
            }           
            else return;
            args[i] = immediates.back().get();
        }
        else
        {
            if(!nodes[wire.nodeIndex].outputValues[wire.pinIndex]) return;
            args[i] = nodes[wire.nodeIndex].outputValues[wire.pinIndex].get();
        }
    }
    n.outputValues = n.nodeType->Evaluate(args);
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

void GraphEditor::DeleteNode(int index)
{
    for(auto & node : nodes)
    {
        for(auto & input : node.inputs)
        {
            if(input.nodeIndex > index) --input.nodeIndex;
            else if(input.nodeIndex == index) input = {-1,-1};
        }
    }
    nodes.erase(begin(nodes) + index);

    clickedFeature = mouseOverFeature = {};
}

void GraphEditor::DeleteSelection()
{
    for(int i = nodes.size()-1; i>=0; --i)
    {
        if(nodes[i].selected) DeleteNode(i);
    }
}
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

Rect Feature::GetPinRect() const
{
    switch(type)
    {
    case Input: return node->GetInputPinRect(pin);
    case Output: return node->GetOutputPinRect(pin);
    case FlowInput: return node->GetFlowInputRect();
    case FlowOutput: return node->GetFlowOutputRect();
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
    case Feature::Input: return b.type == Feature::Output && IsAssignable(b.node->GetOutputType(b.pin), a.node->GetInputType(a.pin));
    case Feature::Output: return b.type == Feature::Input && IsAssignable(a.node->GetOutputType(a.pin), b.node->GetInputType(b.pin));
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
            if(n.IsSequenced())
            {
                rect = n.GetFlowInputRect();
                if(x >= rect.x0 && x < rect.x1 && y >= rect.y0 && y < rect.y1)
                {
                    return {{x,y}, Feature::FlowInput, &n, 0};
                }

                rect = n.GetFlowOutputRect();
                if(x >= rect.x0 && x < rect.x1 && y >= rect.y0 && y < rect.y1)
                {
                    return {{x,y}, Feature::FlowOutput, &n, 0};
                }
            }

            for(size_t i=0; i<n.GetInputCount(); ++i)
            {
                rect = n.GetInputPinRect(i);
                if(x >= rect.x0 && x < rect.x1 && y >= rect.y0 && y < rect.y1)
                {
                    return {{x,y}, Feature::Input, &n, i};
                }
            }

            for(size_t i=0; i<n.GetOutputCount(); ++i)
            {
                rect = n.GetOutputPinRect(i);
                if(x >= rect.x0 && x < rect.x1 && y >= rect.y0 && y < rect.y1)
                {
                    return {{x,y}, Feature::Output, &n, i};
                }
            }

            return {{x,y}, Feature::Body, &n};
        }
    }

    return {{x,y}, Feature::None};
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
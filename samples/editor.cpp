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

Feature GraphEditor::GetFeature(const int2 & coord)
{
    for(auto & n : nodes)
    {
        if(n.GetNodeRect().Contains(coord))
        {
            if(n.HasInFlow())
            {
                if(n.GetFlowInputRect().Contains(coord))
                {
                    return {coord, Feature::FlowInput, &n, 0};
                }
            }

            if(n.HasOutFlow())
            {
                if(n.GetFlowOutputRect().Contains(coord))
                {
                    return {coord, Feature::FlowOutput, &n, 0};
                }
            }

            for(size_t i=0; i<n.GetInputCount(); ++i)
            {
                if(n.GetInputPinRect(i).Contains(coord))
                {
                    return {coord, Feature::Input, &n, i};
                }
            }

            for(size_t i=0; i<n.GetOutputCount(); ++i)
            {
                if(n.GetOutputPinRect(i).Contains(coord))
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



EventExecutionRecord::EventExecutionRecord(const std::vector<Node> & nodes) : nodes(nodes), nodeRecords(nodes.size()), timestamp() 
{

}

void EventExecutionRecord::ExecuteEvent(int nodeIndex)
{
    while(nodeIndex >= 0 && nodeIndex < nodes.size())
    {
        ++timestamp;
        for(auto & input : nodes[nodeIndex].inputs)
        {
            if(input.nodeIndex >= 0) LazilyUpdatePureNode(input.nodeIndex);
        }
        ExecuteNode(nodeIndex);
        nodeIndex = nodes[nodeIndex].flowOutputIndex;
    }
}

void EventExecutionRecord::LazilyUpdatePureNode(int index)    
{
    // If this node is sequenced, simply verify that it has been run at least once
    auto & node = nodes[index];
    if(node.HasInFlow() || node.HasOutFlow())
    {
        if(nodeRecords[index].timestamp == 0) throw std::runtime_error("Sequencing error! Node depends on sequenced node which has not yet been run!");
        return;
    }

    // Otherwise node is pure.
    if(nodeRecords[index].timestamp == timestamp) return; // Node was recomputed this timestep, no need to revisit it

    bool needsUpdate = false;
    if(nodeRecords[index].timestamp == 0) needsUpdate = true; // If this node was never computed, we need to update it

    // Check all dependencies
    for(size_t i=0; i<node.inputs.size(); ++i)
    {
        const auto & input = node.inputs[i];
        if(input.nodeIndex >= 0)
        {
            // Allow this input to update if it needs to. If this input was recomputed more recently than our node, we also need to recompute
            LazilyUpdatePureNode(input.nodeIndex);
            if(nodeRecords[input.nodeIndex].timestamp > nodeRecords[index].timestamp) needsUpdate = true;
        }
    }

    // If this node needs an update, lets recompute it
    if(needsUpdate) ExecuteNode(index);
}

void EventExecutionRecord::ExecuteNode(int index)
{
    const auto & node = nodes[index];
    auto & record = nodeRecords[index];

    void * args[8];
    for(size_t i=0; i<node.inputs.size(); ++i)
    {
        auto wire = node.inputs[i];
        if(wire.nodeIndex < 0)
        {
            if(wire.immediate.empty()) return;

            auto type = node.GetInputType(i);
            assert(type.indirection == VarType::None);
            if(type.type->index == typeid(int))
            {
                std::istringstream ss(wire.immediate);
                int value; if(!(ss >> value)) return;
                allProducedValues.push_back(std::make_shared<int>(value));
            }
            else if(type.type->index == typeid(float))
            {
                std::istringstream ss(wire.immediate);
                float value; if(!(ss >> value)) return;
                allProducedValues.push_back(std::make_shared<float>(value));
            }           
            else return;
            args[i] = allProducedValues.back().get();
        }
        else
        {
            const auto & inputRecord = nodeRecords[wire.nodeIndex];
            assert(inputRecord.timestamp > 0 && inputRecord.timestamp <= timestamp);
            args[i] = nodeRecords[wire.nodeIndex].outputValues[wire.pinIndex].get();
        }
    }

    // Evalute node
    record.outputValues = node.nodeType->Evaluate(args);
    record.timestamp = timestamp;

    // Copy produced values into all produced values list, in case results are later overwritten while the produced values are still referenced
    for(auto value : record.outputValues) allProducedValues.push_back(value);
}
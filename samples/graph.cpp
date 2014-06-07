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

static void LazilyUpdatePureNode(GraphEditor & editor, std::vector<int> & lastTimeComputed, int nodeIndex, int curTime)
{
    // If this node is sequenced, simply verify that it has been run at least once
    auto & node = editor.nodes[nodeIndex];
    if(node.HasInFlow() || node.HasOutFlow())
    {
        if(lastTimeComputed[nodeIndex] == 0) throw std::runtime_error("Flow error: Node depends on sequenced node which has not yet been run!");
        return;
    }

    // Otherwise node is pure.
    int lastTime = lastTimeComputed[nodeIndex];
    if(lastTime == curTime) return; // Node was recomputed this timestep, no need to revisit it

    bool needsUpdate = false;
    if(lastTime == 0) needsUpdate = true; // If this node was never computed, we need to update it

    // Check all dependencies
    for(size_t i=0; i<node.inputs.size(); ++i)
    {
        const auto & input = node.inputs[i];
        if(input.nodeIndex >= 0)
        {
            // Allow this input to update if it needs to. If this input was recomputed more recently than our node, we also need to recompute
            LazilyUpdatePureNode(editor, lastTimeComputed, input.nodeIndex, curTime);
            if(lastTimeComputed[input.nodeIndex] > lastTime) needsUpdate = true;
        }
    }

    // If this node needs an update, lets recompute it
    if(needsUpdate)
    {
        editor.RecomputeNode(node);
        lastTimeComputed[nodeIndex] = curTime;
    }
}

void GraphEditor::ExecuteNode(Node & n)
{
    std::vector<int> lastTimeComputed(nodes.size(), 0);
    int curTime = 0;

    Node * nextNode = &n;    
    while(nextNode)
    {
        ++curTime;

        // Update any of our pure functional dependencies
        for(const auto & input : nextNode->inputs)
        {
            if(input.nodeIndex >= 0) LazilyUpdatePureNode(*this, lastTimeComputed, input.nodeIndex, curTime);
        }

        // Recompute current sequenced node, and record time computed
        RecomputeNode(*nextNode);
        lastTimeComputed[nextNode - nodes.data()] = curTime;

        // Determine next node in sequence
        auto nextIndex = nextNode->flowOutputIndex;
        nextNode = nextIndex < 0 ? nullptr : &nodes[nextIndex];
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
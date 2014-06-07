#include "graph.h"

#include <sstream>

////////////////////////
// Node type creation //
////////////////////////

NodeType NodeType::MakeEventNode(std::string name)
{
    NodeType n = {};
    n.uniqueId = "event:"+name;
    n.label = "On "+name;
    n.hasOutFlow = true;
    n.eval = [](void ** inputs) { return std::vector<std::shared_ptr<void>>{}; };
    return n;        
}

NodeType NodeType::MakeFunctionNode(const Function & function)
{
    NodeType n = {};

    std::ostringstream ss; ss << "func:" << function;
    n.uniqueId = ss.str();
    n.label = function.GetName();
    for(size_t i=0; i<function.GetParamCount(); ++i) n.inputs.push_back({function.GetParamName(i), function.GetParamType(i)});
    if(function.GetReturnType().type->index != typeid(void)) n.outputs.push_back({"", function.GetReturnType()});
    n.hasInFlow = n.hasOutFlow = true;
    n.eval = [&function](void ** inputs) { return std::vector<std::shared_ptr<void>>{function.Invoke(inputs)}; };
    return n;
}

NodeType NodeType::MakeSplitNode(const Type & type)
{
    NodeType n = {};

    std::ostringstream ss; ss << "split:" << type;
    n.uniqueId = ss.str();
    ss.str(""); ss << "split " << type;
    n.label = ss.str();
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

NodeType NodeType::MakeBuildNode(const Type & type)
{
    NodeType n = {};

    std::ostringstream ss; ss << "build:" << type;
    n.uniqueId = ss.str();
    ss.str(""); ss << "build " << type;
    n.label = ss.str();
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

/////////////////////
// Execution logic //
/////////////////////

class EventExecutionRecord
{
    struct NodeRecord
    {
        std::vector<std::shared_ptr<void>> outputValues;
        int timestamp;
        NodeRecord() : timestamp() {}
    };

    const std::vector<Node> & nodes;
    std::vector<NodeRecord> nodeRecords;
    std::vector<std::shared_ptr<void>> allProducedValues;
    int timestamp;

    void ExecuteNode(int index);
    void LazilyUpdatePureNode(int index);
public:
    EventExecutionRecord(const std::vector<Node> & nodes);

    void ExecuteEvent(int nodeIndex);
};

void ExecuteEvent(const std::vector<Node> & nodes, int startIndex)
{
    EventExecutionRecord record(nodes);
    record.ExecuteEvent(startIndex);    
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
    if(node.nodeType->hasInFlow || node.nodeType->hasOutFlow)
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

            auto type = node.nodeType->GetInputType(i);
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



//////////////////////////////
// JSON serialization logic //
//////////////////////////////

#include "json.h"

JsonValue SaveGraph(const std::vector<Node> & nodes)
{
    JsonArray jNodes;
    for(auto & node : nodes)
    {
        JsonObject jNode = {
            {"x", node.x},
            {"y", node.y},
            {"id", node.nodeType->GetUniqueId()}
        };       

        JsonArray jWires;
        for(const auto & wire : node.inputs)
        {
            if(wire.nodeIndex == -1)
            {
                if(wire.immediate.empty()) jWires.push_back(nullptr);
                else jWires.push_back(wire.immediate);
            }
            else jWires.push_back(JsonObject{{"node",wire.nodeIndex}, {"pin",wire.pinIndex}});
        }
        if(!jWires.empty()) jNode.push_back({"wires", jWires});

        if(node.nodeType->hasOutFlow) jNode.push_back({"next", node.flowOutputIndex});

        jNodes.push_back(jNode);
    }
    return jNodes;
}

std::vector<Node> LoadGraph(const std::vector<NodeType> & nodeTypes, const JsonValue & jGraph)
{
    // Create the stored nodes
    std::vector<Node> nodes;
    for(const auto & jNode : jGraph.array())
    {    
        // Search for the correct node type
        const auto & id = jNode["id"].string();
        const NodeType * nodeType = nullptr;
        for(auto & type : nodeTypes)
        {
            if(id == type.GetUniqueId())
            {
                nodeType = &type;
                break;
            }
        }
        if(!nodeType) throw std::runtime_error("Unrecognized node type: "+id);

        // Create the node
        nodes.push_back(Node(*nodeType, jNode["x"].number<int>(), jNode["y"].number<int>()));
        auto & node = nodes.back();

        // Connect the wires
        const auto & jWires = jNode["wires"].array();
        if(jWires.size() != node.inputs.size()) throw std::runtime_error("Node input count mismatch: "+id);
        for(size_t i=0; i<jWires.size(); ++i)
        {
            const auto & jWire = jWires[i];
            if(jWire.isString()) node.inputs[i] = {-1,-1,jWire.string()}; // Immediate value
            else if(jWire.isObject()) node.inputs[i] = {jWire["node"].number<int>(), jWire["pin"].number<int>()}; // Wired to other node
            else node.inputs[i] = {-1,-1}; // Not hooked up yet
        }

        // Connect flow wires
        node.flowOutputIndex = jNode["next"].numberOrDefault(-1);
    }
    assert(nodes.size() == jGraph.array().size());

    // TODO: Validate the graph. At the very least, wire types, pin indices, and flow wiring.

    return nodes;
}
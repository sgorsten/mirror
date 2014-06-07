#include "graph.h"
#include "event.h"  // For Program
#include "json.h"   // For JsonValue

///////////////////////
// Compilation logic //
///////////////////////

class ProgramCompiler
{
    struct NodeRecord
    {
        std::vector<size_t> inputSlots;
        std::vector<size_t> outputSlots;
        bool used;
        int timestamp;
        NodeRecord() : used(), timestamp() {}
    };

    Program & program;
    const std::vector<Node> & nodes;
    std::vector<NodeRecord> nodeRecords;
    std::vector<std::shared_ptr<void>> allProducedValues;
    int timestamp;

    void CompileConstants(int index);
    void ExecuteNode(int index);
    void LazilyUpdatePureNode(int index);
public:
    ProgramCompiler::ProgramCompiler(Program & program, const std::vector<Node> & nodes) : program(program), nodes(nodes), timestamp() {}
    void Compile(int startIndex);
};

std::shared_ptr<const Program> Compile(const std::vector<Node> & nodes, int startIndex)
{
    auto program = std::make_shared<Program>();
    ProgramCompiler compiler(*program, nodes);
    compiler.Compile(startIndex);
    return program;
}

void ProgramCompiler::CompileConstants(int index)
{
    auto & record = nodeRecords[index];
    if(record.used) return; // Only need to do this once per node
    record.used = true;

    auto & node = nodes[index];
    for(size_t i=0; i<node.inputs.size(); ++i)
    {
        auto & input = node.inputs[i];
        if(input.nodeIndex == -1) // Immediate
        {
            if(input.immediate.empty()) throw std::runtime_error("Compile error - Wire not connected and no immediate present!");

            auto type = node.nodeType->inputs[i].type;
            assert(type.indirection == VarType::None);
            if(type.type->index == typeid(int))
            {
                std::istringstream ss(input.immediate);
                int value; if(!(ss >> value)) throw std::runtime_error("Compile error - Unable to parse int from "+input.immediate);
                program.constants.push_back(std::make_shared<int>(value));
            }
            else if(type.type->index == typeid(float))
            {
                std::istringstream ss(input.immediate);
                float value; if(!(ss >> value)) throw std::runtime_error("Compile error - Unable to parse float from "+input.immediate);
                program.constants.push_back(std::make_shared<float>(value));
            }           
            else throw std::runtime_error(std::string("Compile error - Immediates are not supported for ")+type.type->index.name());

            record.inputSlots[i] = program.constants.size()-1;
        }
        else // Wired to other node
        {
            CompileConstants(input.nodeIndex);
        }
    }
}

void ProgramCompiler::Compile(int nodeIndex)
{
    program.calls.clear();
    program.constants.clear();
    program.numTemporarySlots = 0;

    // Reserve input and output slot indices for every node
    nodeRecords.resize(nodes.size());
    for(size_t i=0; i<nodes.size(); ++i)
    {
        nodeRecords[i].used = 0;
        nodeRecords[i].timestamp = 0;
        nodeRecords[i].inputSlots.resize(nodes[i].nodeType->inputs.size());
        nodeRecords[i].outputSlots.resize(nodes[i].nodeType->outputs.size());
    }

    // Compile all constants and mark used nodes
    for(int i = nodeIndex; i != -1; i = nodes[i].flowOutputIndex)
    {
        CompileConstants(i);
    }
    program.numTemporarySlots = program.constants.size();

    // Reserve temporary slots for outputs of all used functions
    for(size_t i=0; i<nodes.size(); ++i)
    {
        if(!nodeRecords[i].used) continue;
        for(size_t j=0; j<nodes[i].nodeType->outputs.size(); ++j)
        {
            nodeRecords[i].outputSlots[j] = program.numTemporarySlots + j;
        }
        program.numTemporarySlots += nodes[i].nodeType->outputs.size();
    }

    // Determine temporary slot indices to use for function inputs
    for(size_t i=0; i<nodes.size(); ++i)
    {
        if(!nodeRecords[i].used) continue;
        for(size_t j=0; j<nodes[i].nodeType->inputs.size(); ++j)
        {
            auto & input = nodes[i].inputs[j];
            if(input.nodeIndex == -1) continue; // Immediates were already set during CompileConstants() phase
            nodeRecords[i].inputSlots[j] = nodeRecords[input.nodeIndex].outputSlots[input.pinIndex];
        }
    }

    // Emit calls for nodes in order
    for(int i = nodeIndex; i != -1; i = nodes[i].flowOutputIndex)
    {
        ++timestamp;
        for(auto & input : nodes[i].inputs)
        {
            if(input.nodeIndex >= 0) LazilyUpdatePureNode(input.nodeIndex);
        }
        ExecuteNode(i);
    }
}

void ProgramCompiler::LazilyUpdatePureNode(int index)    
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

void ProgramCompiler::ExecuteNode(int index)
{
    const auto & node = nodes[index];
    auto & record = nodeRecords[index];

    Program::Call call;
    call.nodeType = node.nodeType;
    call.inputSlotIndices = record.inputSlots;
    call.outputSlotIndices = record.outputSlots;
    program.calls.push_back(call);

    record.timestamp = timestamp;
}

//////////////////////////////
// JSON serialization logic //
//////////////////////////////

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
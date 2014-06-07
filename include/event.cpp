#include "event.h"

#include <cassert>
#include <sstream>

struct NodeType::Impl
{
    std::string uniqueId, label;
    std::vector<NodeType::Pin> inputs, outputs;
    bool hasInFlow;
    bool hasOutFlow;
    std::function<std::vector<std::shared_ptr<void>>(void **)> eval;
};

const std::string & NodeType::GetUniqueId() const { return impl->uniqueId; }
const std::string & NodeType::GetLabel() const { return impl->label; }
const std::vector<NodeType::Pin> & NodeType::GetInputs() const { return impl->inputs; }
const std::vector<NodeType::Pin> & NodeType::GetOutputs() const { return impl->outputs; }
bool NodeType::HasInFlow() const { return impl->hasInFlow; }
bool NodeType::HasOutFlow() const { return impl->hasOutFlow; }

////////////////////////
// Node type creation //
////////////////////////

NodeType NodeType::MakeEventNode(std::string name)
{
    auto impl = std::make_shared<Impl>();
    impl->uniqueId = "event:"+name;
    impl->label = "On "+name;
    impl->hasOutFlow = true;
    impl->eval = [](void ** inputs) { return std::vector<std::shared_ptr<void>>{}; };

    NodeType n;
    n.impl = impl;
    return n;
}

NodeType NodeType::MakeFunctionNode(const Function & function)
{   
    auto impl = std::make_shared<Impl>();
    std::ostringstream ss; ss << "func:" << function;
    impl->uniqueId = ss.str();
    impl->label = function.GetName();
    for(size_t i=0; i<function.GetParamCount(); ++i) impl->inputs.push_back({function.GetParamName(i), function.GetParamType(i)});
    if(function.GetReturnType().type->index != typeid(void)) impl->outputs.push_back({"", function.GetReturnType()});
    impl->hasInFlow = impl->hasOutFlow = true;
    impl->eval = [&function](void ** inputs) { return std::vector<std::shared_ptr<void>>{function.Invoke(inputs)}; };

    NodeType n;
    n.impl = impl;
    return n;
}

NodeType NodeType::MakeSplitNode(const Type & type)
{
    auto impl = std::make_shared<Impl>();
    std::ostringstream ss; ss << "split:" << type;
    impl->uniqueId = ss.str();
    ss.str(""); ss << "split " << type;
    impl->label = ss.str();
    impl->inputs.push_back({"", {&type, false, false, VarType::LValueRef}});
    for(auto & f : type.fields) impl->outputs.push_back({f.identifier, f.type});
    impl->eval = [&type](void ** inputs) 
    {
        std::vector<std::shared_ptr<void>> outputs; 
        for(auto & field : type.fields) outputs.push_back(std::shared_ptr<void>(field.accessor(inputs[0]), [](void *){}));
        return outputs; 
    };

    NodeType n;
    n.impl = impl;
    return n;
}

NodeType NodeType::MakeBuildNode(const Type & type)
{
    auto impl = std::make_shared<Impl>();
    std::ostringstream ss; ss << "build:" << type;
    impl->uniqueId = ss.str();
    ss.str(""); ss << "build " << type;
    impl->label = ss.str();
    for(auto & f : type.fields) impl->inputs.push_back({f.identifier, f.type});
    impl->outputs.push_back({"", {&type, false, false, VarType::None}});
    impl->eval = [&type](void ** inputs) -> std::vector<std::shared_ptr<void>>
    {
        auto output = type.DefConstruct();
        for(auto & field : type.fields)
        {
            assert(field.type.indirection == VarType::None);
            field.type.type->CopyAssign(field.accessor(output.get()), *inputs++);
        }
        return {output};
    };

    NodeType n;
    n.impl = impl;
    return n;
}

///////////////////////
// Program execution //
///////////////////////

struct Program::Impl
{
    std::vector<Line>                   lines;              // List of calls to be made
    std::vector<std::shared_ptr<void>>  constants;          // Program constants, which occupy the first set of temporary slots
    size_t                              numTemporarySlots;  // Total number of temporary slots needed
};

Program Program::Load(std::vector<std::shared_ptr<void>> constants, std::vector<Line> lines)
{
    // Validate that lines only read from previously written slots
    std::vector<bool> isSlotWritten(constants.size(), true);
    for(const auto & line : lines)
    {
        // Check inputs
        for(auto slot : line.inputs)
        {
            if(slot >= isSlotWritten.size()) isSlotWritten.resize(slot+1, false);
            if(!isSlotWritten[slot]) throw std::runtime_error("Ill-formed program: Line reads from slot that has not been written to!");
        }

        // Mark outputs as written
        for(auto slot : line.outputs)
        {
            if(slot >= isSlotWritten.size()) isSlotWritten.resize(slot+1, false);
            isSlotWritten[slot] = true;
        }
    }
    // TODO: Validate types (perhaps store written type instead of simply a bool)

    auto impl = std::make_shared<Impl>();
    impl->lines = move(lines);
    impl->constants = move(constants);
    impl->numTemporarySlots = isSlotWritten.size();

    Program p;
    p.impl = impl;
    return p;
}

void Program::operator()() const
{
    if(!impl) return;

    // Reserve enough space
    auto slotValues = impl->constants;
    auto allValues = slotValues;
    slotValues.resize(impl->numTemporarySlots, nullptr);
    
    // Execute lines in order
    for(auto & line : impl->lines)
    {
        // Setup arguments list
        void * args[8];
        for(size_t i=0; i<line.inputs.size(); ++i)
        {
            args[i] = slotValues[line.inputs[i]].get();
            assert(args[i] != nullptr);
        }

        // Evalute node
        auto outputs = line.type.impl->eval(args);

        // Store outputs
        allValues.insert(end(allValues), begin(outputs), end(outputs));
        for(size_t i=0; i<line.outputs.size(); ++i)
        {
            slotValues[line.outputs[i]] = outputs[i];
        }
    } 
}
#include "event.h"

#include <cassert>
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

///////////////////////
// Program execution //
///////////////////////

void Program::Execute() const
{
    // Reserve enough space
    auto slotValues = constants;
    auto allValues = slotValues;
    slotValues.resize(numTemporarySlots, nullptr);
    
    // Execute calls in order
    for(auto & call : calls)
    {
        // Setup arguments list
        void * args[8];
        for(size_t i=0; i<call.inputSlotIndices.size(); ++i)
        {
            args[i] = slotValues[call.inputSlotIndices[i]].get();
            assert(args[i] != nullptr);
        }

        // Evalute node
        auto outputs = call.nodeType->eval(args);

        // Store outputs
        allValues.insert(end(allValues), begin(outputs), end(outputs));
        for(size_t i=0; i<call.outputSlotIndices.size(); ++i)
        {
            slotValues[call.outputSlotIndices[i]] = outputs[i];
        }
    } 
}
#pragma once

// calc_nodes — shared compute-node fixture for the calc_graph example and the
// integration test. Single source of truth (DRY), owned by neither client: the
// test validates the exact node types the example ships. Lives under toolkit/
// fixtures/ (not examples/ or tests/) so neither consumer's layout is load-bearing
// for the other.
//
// These demonstrate the toolkit's pull-on-evaluate idiom. Connector carries no
// value payload in 0.x (that push/pull machinery is deferred; see node.hpp), so
// a node reads its upstream on demand inside evaluate(): walk
// input(i)->links().front()->node(), node_cast to the producer base (ValueNode),
// read its `result`. NodeGraph::evaluate() is a topological walk, so a producer's
// result is already fresh by the time a consumer reads it.
//
// Header-only; depends only on the node data-model headers (imgui-free), so it
// links into both the backend-light test and the Application-layer example.

#include <nlohmann/json.hpp>

#include <imtool/node/node.hpp>
#include <imtool/node/node_registry.hpp>
#include <imtool/settings/setting.hpp>   // NumberNode exposes its value as a Setting

namespace imtool::calc {

// Base for value-producing nodes: holds the computed result. Subclasses override
// compute() (single responsibility); evaluate() runs it in topological order.
struct ValueNode : Node {
    using Node::Node;
    float result = 0.0f;
    virtual void compute() {}
    bool evaluate() override { compute(); return true; }
};

// Read a float-typed input port's upstream value (0 if unconnected / wrong type).
[[nodiscard]] inline float upstream(const Node *n, int port) {
    const Connector *in = n->input(port);
    if(!in || !in->connected()) { return 0.0f; }
    const Node *up = in->links().front()->node();
    if(const auto *v = node_cast<ValueNode>(up)) { return v->result; }
    return 0.0f;
}

// A constant source. `value` is real subclass state, persisted via the params
// hooks (into the per-node "params" sub-object) so it survives a save/load.
struct NumberNode : ValueNode {
    float value = 0.0f;
    NumberNode() : ValueNode("Number") {
        addOutput<float>("value");
        // Epic B: a Setting observes `value`, so the inspector edits the same field
        // compute() reads, and it serializes via the base saveParams shim (the JSON
        // is byte-identical to the old manual p["value"]=value).
        settings().add<float>("value", "Value", &value);
    }
    void compute() override { result = value; }
};

struct AddNode : ValueNode {
    AddNode() : ValueNode("Add") { addInput<float>("a"); addInput<float>("b"); addOutput<float>("sum"); }
    void compute() override { result = upstream(this, 0) + upstream(this, 1); }
};

struct MulNode : ValueNode {
    MulNode() : ValueNode("Mul") { addInput<float>("a"); addInput<float>("b"); addOutput<float>("product"); }
    void compute() override { result = upstream(this, 0) * upstream(this, 1); }
};

struct OutputNode : ValueNode {
    OutputNode() : ValueNode("Output") { addInput<float>("in"); }
    void compute() override { result = upstream(this, 0); }
};

// Register the calc node types (Number/Add/Mul/Output) on a registry.
inline void registerNodes(NodeRegistry &reg) {
    reg.addType<NumberNode>("Number");
    reg.addType<AddNode>("Add");
    reg.addType<MulNode>("Mul");
    reg.addType<OutputNode>("Output");
}

}  // namespace imtool::calc

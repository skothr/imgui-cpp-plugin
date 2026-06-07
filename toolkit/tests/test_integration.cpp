#include "imtest.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include <imtool/node/node.hpp>
#include <imtool/node/node_graph.hpp>
#include <imtool/node/node_registry.hpp>

// Integration test — drives the toolkit the way a consumer (e.g. agent-nexus)
// would: register real *compute* node types, wire them into a graph, evaluate(),
// and assert the numbers that come out. Unlike the unit tests (which mock the
// node behavior with empty test doubles), this exercises the actual data-flow
// idiom the toolkit supports.
//
// Data-flow model (important): Connector carries NO value payload — that push/
// pull machinery is deferred post-beta (see node.hpp). So a compute node reads
// its upstream on demand inside evaluate(): walk input(i)->links().front()->node(),
// downcast to the known producer base (ValueNode), read its `result`. Because
// NodeGraph::evaluate() is a Kahn topological walk, every producer's evaluate()
// runs before its consumer's, so the upstream `result` is already fresh.
//
// This file also closes two coverage gaps the review flagged:
//   - the "params" sub-object JSON round-trip for a subclass with real params,
//     including the reserved-key collision-avoidance guarantee (a param literally
//     named "pos"/"id" must not clobber the node's real pos/id);
//   - diamond / multi-fan-in topological evaluation order (a node downstream of
//     two branches must not evaluate until BOTH branches have).

using namespace imtool;
using nlohmann::json;

namespace {

// Base for every value-producing node: holds the computed result + an optional
// evaluation-order trace hook the test installs to assert topo ordering.
struct ValueNode : Node {
    using Node::Node;
    float             result    = 0.0f;
    std::vector<int> *evalOrder = nullptr;   // test hook (not serialized)

    bool evaluate() override {
        compute();
        if(evalOrder) { evalOrder->push_back(id()); }
        return true;
    }
    virtual void compute() {}
};

// Read a float-typed input port's upstream value (0 if unconnected / wrong type).
float upstream(const Node *n, int port) {
    const Connector *in = n->input(port);
    if(!in || !in->connected()) { return 0.0f; }
    const Node *up = in->links().front()->node();
    if(const auto *v = dynamic_cast<const ValueNode*>(up)) { return v->result; }
    return 0.0f;
}

// A constant source. Its `value` is real subclass state, persisted via the
// params hooks so it survives a save/load round-trip.
struct NumberNode : ValueNode {
    float value = 0.0f;
    NumberNode() : ValueNode("Number") { addOutput<float>("value"); }
    void compute() override { result = value; }
    void saveParams(json &p) const override { p["value"] = value; }
    void loadParams(const json &p) override { value = p.value("value", 0.0f); }
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

// Exercises the reserved-key collision guarantee: its params are deliberately
// named "pos" and "id" — the exact reserved node keys. They must land in the
// per-node "params" sub-object and never clobber the node's real pos / id.
struct TaggedNode : ValueNode {
    std::string tag  = "default";
    int         code = 0;
    TaggedNode() : ValueNode("Tagged") { addOutput<float>("out"); }
    void saveParams(json &p) const override { p["pos"] = tag; p["id"] = code; }
    void loadParams(const json &p) override {
        tag  = p.value("pos", std::string{});
        code = p.value("id", 0);
    }
};

void registerAll(NodeRegistry &reg) {
    reg.addType<NumberNode>("Number");
    reg.addType<AddNode>("Add");
    reg.addType<MulNode>("Mul");
    reg.addType<OutputNode>("Output");
    reg.addType<TaggedNode>("Tagged");
}

template<typename T>
T *as(Node *n) { return dynamic_cast<T*>(n); }

}  // namespace

int main() {
    IMTEST_SUITE("integration");

    // --- 1. linear calculator: (5 + 3) -> Output = 8 -----------------------
    {
        NodeRegistry reg; registerAll(reg);
        NodeGraph g(&reg);
        auto *a   = as<NumberNode>(g.create("Number")); a->value = 5.0f;
        auto *b   = as<NumberNode>(g.create("Number")); b->value = 3.0f;
        auto *add = g.create("Add");
        auto *out = as<OutputNode>(g.create("Output"));
        CHECK(a && b && add && out);
        CHECK(g.connect(a->output(0),   add->input(0)));
        CHECK(g.connect(b->output(0),   add->input(1)));
        CHECK(g.connect(add->output(0), out->input(0)));
        CHECK(g.evaluate());
        CHECK_NEAR(out->result, 8.0f, 1e-6);
        std::printf("  [calc]    5 + 3            = %.1f\n", static_cast<double>(out->result));
    }

    // --- 2. diamond: src=4; L=src+src=8; R=src*src=16; sink=L+R=24 ----------
    //     verifies fan-out + that `sink` evaluates only after BOTH L and R.
    {
        NodeRegistry reg; registerAll(reg);
        NodeGraph g(&reg);
        std::vector<int> order;

        auto *src  = as<NumberNode>(g.create("Number")); src->value = 4.0f;
        auto *left = as<AddNode>(g.create("Add"));
        auto *right= as<MulNode>(g.create("Mul"));
        auto *sink = as<AddNode>(g.create("Add"));
        for(ValueNode *v : {static_cast<ValueNode*>(src), static_cast<ValueNode*>(left),
                            static_cast<ValueNode*>(right), static_cast<ValueNode*>(sink)}) {
            v->evalOrder = &order;
        }
        // src fans out to both branches' inputs (outputs fan out; inputs exclusive).
        CHECK(g.connect(src->output(0),  left->input(0)));
        CHECK(g.connect(src->output(0),  left->input(1)));
        CHECK(g.connect(src->output(0),  right->input(0)));
        CHECK(g.connect(src->output(0),  right->input(1)));
        CHECK(g.connect(left->output(0), sink->input(0)));
        CHECK(g.connect(right->output(0),sink->input(1)));
        CHECK(g.edgeCount() == 6);

        CHECK(g.evaluate());
        CHECK_NEAR(sink->result, 24.0f, 1e-6);     // (4+4) + (4*4) = 8 + 16

        // topo property: src first; sink strictly after both left and right.
        const auto pos = [&](int id) {
            return static_cast<int>(std::find(order.begin(), order.end(), id) - order.begin());
        };
        CHECK(order.size() == 4);
        CHECK(pos(src->id())  < pos(left->id()));
        CHECK(pos(src->id())  < pos(right->id()));
        CHECK(pos(left->id()) < pos(sink->id()));
        CHECK(pos(right->id())< pos(sink->id()));
        std::printf("  [diamond] (4+4) + (4*4)    = %.1f  (eval order ok)\n", static_cast<double>(sink->result));
    }

    // --- 3. params JSON round-trip: rebuild a graph from JSON and recompute --
    //     if NumberNode.value did not survive in the "params" sub-object, the
    //     reconstructed sum would be 0, not 8.
    {
        NodeRegistry reg; registerAll(reg);
        NodeGraph src(&reg);
        auto *a   = as<NumberNode>(src.create("Number")); a->value = 5.0f; a->setPos(Vec2f(-10.0f, 20.0f));
        auto *b   = as<NumberNode>(src.create("Number")); b->value = 3.0f;
        auto *add = src.create("Add");
        auto *out = src.create("Output");
        src.connect(a->output(0),   add->input(0));
        src.connect(b->output(0),   add->input(1));
        src.connect(add->output(0), out->input(0));
        const int outId = out->id();

        const json saved = src.toJson();

        NodeGraph dst(&reg);
        CHECK(dst.fromJson(saved));
        CHECK(dst.nodeCount() == 4);
        CHECK(dst.edgeCount() == 3);
        CHECK(dst.evaluate());
        auto *outR = as<OutputNode>(dst.find(outId));
        CHECK(outR != nullptr);
        CHECK_NEAR(outR->result, 8.0f, 1e-6);       // value param round-tripped
        auto *aR = as<NumberNode>(dst.find(a->id()));
        CHECK(aR != nullptr);
        CHECK_NEAR(aR->value, 5.0f, 1e-6);
        CHECK_NEAR(aR->pos().x, -10.0f, 1e-4);      // reserved pos round-tripped
        std::printf("  [json]    round-trip recompute = %.1f\n", static_cast<double>(outR->result));
    }

    // --- 4. reserved-key collision: params named "pos"/"id" must not clobber
    //     the node's real pos/id, and vice versa ------------------------------
    {
        NodeRegistry reg; registerAll(reg);
        NodeGraph src(&reg);
        auto *t = as<TaggedNode>(src.create("Tagged"));
        CHECK(t != nullptr);
        t->setPos(Vec2f(12.0f, 34.0f));   // real pos
        t->tag  = "hello";                // param literally named "pos"
        t->code = 999;                    // param literally named "id"
        const int realId = t->id();

        const json saved = src.toJson();
        // The reserved keys hold the node identity; the params sub-object holds
        // the same-named user params — they coexist without collision.
        const json &jn = saved["nodes"][0];
        CHECK(jn["id"].get<int>() == realId);
        CHECK_NEAR(jn["pos"][0].get<float>(), 12.0f, 1e-4);
        CHECK(jn["params"]["pos"].get<std::string>() == "hello");
        CHECK(jn["params"]["id"].get<int>() == 999);

        NodeGraph dst(&reg);
        CHECK(dst.fromJson(saved));
        auto *tr = as<TaggedNode>(dst.find(realId));
        CHECK(tr != nullptr);
        CHECK(tr->id() == realId);                 // reserved id intact
        CHECK_NEAR(tr->pos().x, 12.0f, 1e-4);      // reserved pos intact
        CHECK_NEAR(tr->pos().y, 34.0f, 1e-4);
        CHECK(tr->tag == "hello");                 // param "pos" intact (no clobber)
        CHECK(tr->code == 999);                    // param "id" intact (no clobber)
        std::printf("  [collide] param 'pos'=%s real pos=(%.0f,%.0f) — no collision\n",
                    tr->tag.c_str(), static_cast<double>(tr->pos().x), static_cast<double>(tr->pos().y));
    }

    return imtest::report();
}

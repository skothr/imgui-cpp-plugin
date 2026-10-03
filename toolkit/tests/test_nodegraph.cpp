#include "imtest.hpp"

#include <algorithm>
#include <memory>
#include <vector>

#include <imtool/node/node.hpp>
#include <imtool/node/node_graph.hpp>
#include <imtool/node/node_registry.hpp>

using namespace imtool;

namespace {

struct FloatNode : Node {
    FloatNode() : Node("float") { addInput<float>("in"); addOutput<float>("out"); }
};
struct IntNode : Node {
    IntNode() : Node("int") { addInput<int>("in"); addOutput<int>("out"); }
};

// Records evaluation order so topo ordering can be asserted.
struct RecordingNode : Node {
    std::vector<int> *order;
    int               tag;
    RecordingNode(std::vector<int> *o, int t) : Node("rec"), order(o), tag(t) {
        addInput<float>("in");
        addOutput<float>("out");
    }
    bool evaluate() override { order->push_back(tag); return true; }
};

}  // namespace

int main() {
    IMTEST_SUITE("nodegraph");

    NodeRegistry reg;
    reg.addType<FloatNode>("Float");
    reg.addType<IntNode>("Int");
    CHECK(reg.contains("Float"));
    CHECK(reg.typeNames().size() == 2);

    NodeGraph g(&reg);
    Node *a = g.create("Float");
    Node *b = g.create("Float");
    Node *c = g.create("Int");
    CHECK(a && b && c);
    CHECK(g.nodeCount() == 3);
    CHECK(a->typeName() == "Float");
    CHECK(g.create("Nope") == nullptr);     // unknown type
    CHECK(g.find(a->id()) == a);

    // valid connection: a.out(float) -> b.in(float)
    CHECK(g.connect(a->output(0), b->input(0)));
    CHECK(a->output(0)->connected());
    CHECK(b->input(0)->connected());
    CHECK(g.edgeCount() == 1);

    // rejected connections
    CHECK(!g.connect(b->input(0), a->output(0)));   // wrong direction (input as 'out')
    CHECK(!g.connect(a->output(0), c->input(0)));   // type mismatch float vs int
    CHECK(!g.connect(a->output(0), a->input(0)));   // self-loop
    CHECK(!g.connect(a->output(0), b->input(0)));   // exclusive input already wired (dup/exclusive)
    CHECK(!g.connect(b->output(0), a->input(0)));   // would create a cycle (a->b exists)
    CHECK(g.edgeCount() == 1);                       // none of the rejects wired anything

    // disconnect
    g.disconnect(a->output(0), b->input(0));
    CHECK(!b->input(0)->connected());
    CHECK(g.edgeCount() == 0);

    // remove cleans up incident edges
    g.connect(a->output(0), b->input(0));
    g.remove(a);
    CHECK(g.nodeCount() == 2);
    CHECK(!b->input(0)->connected());               // edge removed with node a

    // --- topo evaluation order ---
    {
        std::vector<int> order;
        NodeGraph e;
        Node *n1 = e.add(std::make_unique<RecordingNode>(&order, 1));
        Node *n2 = e.add(std::make_unique<RecordingNode>(&order, 2));
        Node *n3 = e.add(std::make_unique<RecordingNode>(&order, 3));
        CHECK(e.connect(n1->output(0), n2->input(0)));
        CHECK(e.connect(n2->output(0), n3->input(0)));
        CHECK(e.evaluate());
        CHECK(order.size() == 3);
        const auto pos = [&](int t) {
            return std::find(order.begin(), order.end(), t) - order.begin();
        };
        CHECK(pos(1) < pos(2));
        CHECK(pos(2) < pos(3));
    }

    // --- JSON round-trip ---
    {
        NodeGraph src(&reg);
        Node *x = src.create("Float");
        Node *y = src.create("Float");
        x->setPos(Vec2f(12.0f, 34.0f));
        src.connect(x->output(0), y->input(0));
        const nlohmann::json js = src.toJson();

        NodeGraph dst(&reg);
        CHECK(dst.fromJson(js));
        CHECK(dst.nodeCount() == 2);
        CHECK(dst.edgeCount() == 1);
        Node *xr = dst.find(x->id());
        CHECK(xr != nullptr);
        CHECK_NEAR(xr->pos().x, 12.0f, 1e-4);
        CHECK_NEAR(xr->pos().y, 34.0f, 1e-4);
    }

    // --- malformed JSON degrades gracefully (no crash/throw) ---
    // Regression for the adversarial-review findings: fromJson must log+skip bad
    // input, never dereference a missing key (UB) or let a type_error escape.
    {
        using nlohmann::json;
        NodeGraph m(&reg);
        CHECK(!m.fromJson(json::array()));                       // non-object root
        CHECK(!m.fromJson(json{{"nodes", 5}}));                  // "nodes" not an array
        CHECK(m.fromJson(json{{"nodes", json::array()}}));        // empty graph is valid
        // non-object/unknown node entries skipped; edges with missing/garbled keys skipped
        CHECK(m.fromJson(json::parse(
            R"({"nodes":[5,"x",{}],"edges":[{},{"from":5},{"from":{"node":0},"to":{"node":1}}]})")));
        // wrong-typed reserved field caught per-entry, not thrown
        CHECK(m.fromJson(json::parse(R"({"nodes":[{"type":"Float","id":"notint"}]})")));
        CHECK(m.nodeCount() == 0);   // that node was skipped, nothing wired
    }

    return imtest::report();
}

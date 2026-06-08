#include "imtest.hpp"

#include <string>

#include <imtool/node/node_graph.hpp>

#include <calc_nodes.hpp>   // shared compute nodes (Number/Add/Mul/Output) — also drive the calc_graph example

// Integration test — drives the toolkit the way a consumer (e.g. agent-nexus)
// would: register real *compute* node types (the same ones the calc_graph example
// ships, from calc_nodes.hpp), wire them into a graph, evaluate(), and assert the
// numbers that come out. Unlike the unit tests (which mock the node behavior with
// empty test doubles), this exercises the actual data-flow idiom the toolkit
// supports — see calc_nodes.hpp for the pull-on-evaluate model.
//
// This file also closes two coverage gaps the review flagged:
//   - the "params" sub-object JSON round-trip for a subclass with real params,
//     including the reserved-key collision-avoidance guarantee (a param literally
//     named "pos"/"id" must not clobber the node's real pos/id);
//   - diamond / multi-fan-in topological evaluation order (a node downstream of
//     two branches must not evaluate until BOTH branches have).

using namespace imtool;
using namespace imtool::calc;
using nlohmann::json;

namespace {

// Test-only node exercising the reserved-key collision guarantee: its params are
// deliberately named "pos" and "id" — the exact reserved node keys. They must land
// in the per-node "params" sub-object and never clobber the node's real pos / id.
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

// The shared calc node types plus the test-only TaggedNode.
void registerAll(NodeRegistry &reg) {
    registerNodes(reg);             // Number/Add/Mul/Output (calc_nodes.hpp)
    reg.addType<TaggedNode>("Tagged");
}

}  // namespace

int main() {
    IMTEST_SUITE("integration");

    // --- 1. linear calculator: (5 + 3) -> Output = 8 -----------------------
    {
        NodeRegistry reg; registerAll(reg);
        NodeGraph g(&reg);
        auto *a   = node_cast<NumberNode>(g.create("Number")); a->value = 5.0f;
        auto *b   = node_cast<NumberNode>(g.create("Number")); b->value = 3.0f;
        auto *add = g.create("Add");
        auto *out = node_cast<OutputNode>(g.create("Output"));
        CHECK(a && b && add && out);
        CHECK(g.connect(a->output(0),   add->input(0)));
        CHECK(g.connect(b->output(0),   add->input(1)));
        CHECK(g.connect(add->output(0), out->input(0)));
        CHECK(g.evaluate());
        CHECK_NEAR(out->result, 8.0f, 1e-6);
        std::printf("  [calc]    5 + 3            = %.1f\n", static_cast<double>(out->result));
    }

    // --- 2. diamond: src=4; L=src+src=8; R=src*src=16; sink=L+R=24 ----------
    //     fan-out + fan-in. The numeric result IS the topological-order witness:
    //     sink reads left.result + right.result, and a ValueNode's result defaults
    //     to 0, so if sink evaluated before either branch it would read 0 and yield
    //     < 24. 24 is reachable only when src, then BOTH branches, then sink run in
    //     dependency order — exactly the fan-in indegree-decrement path in evaluate().
    {
        NodeRegistry reg; registerAll(reg);
        NodeGraph g(&reg);

        auto *src  = node_cast<NumberNode>(g.create("Number")); src->value = 4.0f;
        auto *left = node_cast<AddNode>(g.create("Add"));
        auto *right= node_cast<MulNode>(g.create("Mul"));
        auto *sink = node_cast<AddNode>(g.create("Add"));
        // src fans out to both branches' inputs (outputs fan out; inputs exclusive).
        CHECK(g.connect(src->output(0),  left->input(0)));
        CHECK(g.connect(src->output(0),  left->input(1)));
        CHECK(g.connect(src->output(0),  right->input(0)));
        CHECK(g.connect(src->output(0),  right->input(1)));
        CHECK(g.connect(left->output(0), sink->input(0)));
        CHECK(g.connect(right->output(0),sink->input(1)));
        CHECK(g.edgeCount() == 6);

        CHECK(g.evaluate());
        CHECK_NEAR(sink->result, 24.0f, 1e-6);     // (4+4) + (4*4); only reachable under correct fan-in topo order
        std::printf("  [diamond] (4+4) + (4*4)    = %.1f  (fan-in topo order ok)\n", static_cast<double>(sink->result));
    }

    // --- 3. params JSON round-trip: rebuild a graph from JSON and recompute --
    //     if NumberNode.value did not survive in the "params" sub-object, the
    //     reconstructed sum would be 0, not 8.
    {
        NodeRegistry reg; registerAll(reg);
        NodeGraph src(&reg);
        auto *a   = node_cast<NumberNode>(src.create("Number")); a->value = 5.0f; a->setPos(Vec2f(-10.0f, 20.0f));
        auto *b   = node_cast<NumberNode>(src.create("Number")); b->value = 3.0f;
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
        auto *outR = node_cast<OutputNode>(dst.find(outId));
        CHECK(outR != nullptr);
        CHECK_NEAR(outR->result, 8.0f, 1e-6);       // value param round-tripped
        auto *aR = node_cast<NumberNode>(dst.find(a->id()));
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
        auto *t = node_cast<TaggedNode>(src.create("Tagged"));
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
        auto *tr = node_cast<TaggedNode>(dst.find(realId));
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

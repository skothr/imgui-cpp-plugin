// calc_graph — a runnable imtool example (Epic H).
//
// Shows the toolkit composed the way a consumer app (e.g. agent-nexus) would:
//   imtool::Application   — owns the GLFW window + GL + ImGui context + frame loop
//   imtool::NodeGraph     — the data model + topological evaluate()
//   NodeGraphDisplay      — the interactive canvas (pan/zoom, drag-to-connect)
//   KeyBindingManager     — configurable shortcuts with a built-in editor
//
// Unlike the bootstrap scaffold's topology-only demo nodes, these are *compute*
// nodes: a node reads its upstream inside evaluate() and produces a value, so the
// Output node displays a number that updates live as you drag the inputs. This is
// the canonical pull-on-evaluate idiom (Connector carries no payload in 0.x; see
// node.hpp). Read this alongside toolkit/tests/test_integration.cpp, which asserts
// the same pipeline headlessly.
//
// Build: configure the enclosing tree with -DIMTOOL_BUILD_APP=ON
// -DIMTOOL_BUILD_EXAMPLES=ON (see smoke/CMakeLists.txt -DIMTOOL_SMOKE_WITH_APP=ON
// for an offline, self-contained build harness).

#include <imgui.h>

#include <imtool/app/application.hpp>
#include <imtool/common/version.hpp>
#include <imtool/input/key_binding.hpp>
#include <imtool/node/node.hpp>
#include <imtool/node/node_graph.hpp>
#include <imtool/node/node_graph_display.hpp>
#include <imtool/node/node_registry.hpp>

using namespace imtool;

// --- compute nodes ---------------------------------------------------------
// ValueNode holds the result; a consumer reads an upstream value by walking the
// link and downcasting. The graph evaluates in topological order, so an
// upstream node's result is fresh by the time a downstream node reads it.
namespace {

struct ValueNode : Node {
    using Node::Node;
    float result = 0.0f;
    virtual void compute() {}
    bool evaluate() override { compute(); return true; }
};

float upstream(const Node *n, int port) {
    const Connector *in = n->input(port);
    if(!in || !in->connected()) { return 0.0f; }
    const Node *up = in->links().front()->node();
    if(const auto *v = dynamic_cast<const ValueNode*>(up)) { return v->result; }
    return 0.0f;
}

struct NumberNode : ValueNode {
    float value = 0.0f;
    NumberNode() : ValueNode("Number") { addOutput<float>("value"); }
    void compute() override { result = value; }
    void saveParams(nlohmann::json &p) const override { p["value"] = value; }
    void loadParams(const nlohmann::json &p) override { value = p.value("value", 0.0f); }
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

}  // namespace

class CalcApp : public Application {
public:
    CalcApp() : Application(makeConfig()) {
        m_registry.addType<NumberNode>("Number");
        m_registry.addType<AddNode>("Add");
        m_registry.addType<MulNode>("Mul");
        m_registry.addType<OutputNode>("Output");
        m_graph.setRegistry(&m_registry);
        m_view.setGraph(&m_graph);

        // Seed (a + b) -> Output, keeping editable handles to the inputs.
        m_a   = dynamic_cast<NumberNode*>(m_graph.create("Number"));
        m_b   = dynamic_cast<NumberNode*>(m_graph.create("Number"));
        auto *add = m_graph.create("Add");
        m_out = dynamic_cast<OutputNode*>(m_graph.create("Output"));
        if(m_a)  { m_a->value = 5.0f; m_a->setPos(Vec2f(-240.0f, -70.0f)); }
        if(m_b)  { m_b->value = 3.0f; m_b->setPos(Vec2f(-240.0f,  70.0f)); }
        if(add)  { add->setPos(Vec2f(-20.0f, 0.0f)); }
        if(m_out){ m_out->setPos(Vec2f(200.0f, 0.0f)); }
        if(m_a && add)   { m_graph.connect(m_a->output(0),  add->input(0)); }
        if(m_b && add)   { m_graph.connect(m_b->output(0),  add->input(1)); }
        if(add && m_out) { m_graph.connect(add->output(0),  m_out->input(0)); }

        m_keys.registerAll({
            {"graph.eval", "Evaluate",   "Recompute the graph",       ImGuiMod_Ctrl | ImGuiKey_R, false, nullptr},
            {"view.reset", "Reset view", "Recenter the node canvas",  ImGuiMod_Ctrl | ImGuiKey_0, false, nullptr},
            {"app.quit",   "Quit",       "Close the application",     ImGuiMod_Ctrl | ImGuiKey_Q, false, nullptr},
        });
    }

protected:
    bool onInit() override {
        log() << LogLevel::Info << "calc_graph ready (imtool v" << version_string() << ")"; log().flush();
        return true;
    }

    void onGui() override {
        if(m_keys.triggered("view.reset")) { m_view.resetView(); }
        if(m_keys.triggered("app.quit"))   { requestQuit(); }

        m_graph.evaluate();   // cheap; keeps m_out->result live as inputs change

        ImGui::SetNextWindowSize(ImVec2(1000.0f, 640.0f), ImGuiCond_FirstUseEver);
        if(ImGui::Begin("imtool calc_graph example")) {
            ImGui::TextDisabled("A node graph that computes (a + b). Drag the inputs; the result updates live.");
            if(m_a) { ImGui::DragFloat("a", &m_a->value, 0.1f); }
            if(m_b) { ImGui::DragFloat("b", &m_b->value, 0.1f); }
            ImGui::SameLine();
            if(ImGui::Button("Reset view")) { m_view.resetView(); }
            ImGui::Text("result a + b = %.2f", m_out ? static_cast<double>(m_out->result) : 0.0);
            ImGui::Text("nodes %zu   edges %zu   FPS %.0f",
                        m_graph.nodeCount(), m_graph.edgeCount(),
                        static_cast<double>(ImGui::GetIO().Framerate));
            ImGui::TextDisabled("right-click: add node | drag port->port: connect | wheel: zoom | Del: remove");
            m_view.draw();   // fills the remaining content region
        }
        ImGui::End();

        m_keys.drawEditor("Key Bindings");
    }

private:
    static AppConfig makeConfig() {
        AppConfig c;
        c.title = "imtool calc_graph";
        c.size  = Vec2i(1280, 800);
        return c;
    }

    NodeRegistry      m_registry;
    NodeGraph         m_graph;
    NodeGraphDisplay  m_view;
    KeyBindingManager m_keys;
    NumberNode       *m_a   = nullptr;
    NumberNode       *m_b   = nullptr;
    OutputNode       *m_out = nullptr;
};

int main() {
    CalcApp app;
    return app.run();   // 0 on clean exit; non-zero == imtool::AppStatus of the failing init step
}

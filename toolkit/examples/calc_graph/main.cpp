// calc_graph — a runnable imtool example (Epic H).
//
// Shows the toolkit composed the way a consumer app (e.g. agent-nexus) would:
//   imtool::Application   — owns the GLFW window + GL + ImGui context + frame loop
//   imtool::NodeGraph     — the data model + topological evaluate()
//   NodeGraphDisplay      — the interactive canvas (pan/zoom, drag-to-connect)
//   KeyBindingManager     — configurable shortcuts with a built-in editor
//
// Unlike the bootstrap scaffold's topology-only demo nodes, these are *compute*
// nodes (defined in the shared calc_nodes.hpp): a node reads its upstream inside
// evaluate() and produces a value, so the Output node displays a number that
// updates live as you drag the inputs. The same calc_nodes.hpp is driven
// headlessly by toolkit/tests/test_integration.cpp, so the test validates the
// exact node types this example ships.
//
// Build: configure the enclosing tree with -DIMTOOL_BUILD_APP=ON
// -DIMTOOL_BUILD_EXAMPLES=ON (see smoke/CMakeLists.txt -DIMTOOL_SMOKE_WITH_APP=ON
// for an offline, self-contained build harness).

#include <imgui.h>

#include <imtool/app/application.hpp>
#include <imtool/common/version.hpp>
#include <imtool/input/key_binding.hpp>
#include <imtool/node/node_graph.hpp>
#include <imtool/node/node_graph_display.hpp>
#include <imtool/node/node_registry.hpp>

#include <calc_nodes.hpp>   // shared compute nodes (Number/Add/Mul/Output) — also driven by test_integration.cpp

using namespace imtool;
using namespace imtool::calc;

class CalcApp : public Application {
public:
    CalcApp() : Application(makeConfig()) {
        registerNodes(m_registry);   // Number/Add/Mul/Output (calc_nodes.hpp)
        m_graph.setRegistry(&m_registry);
        m_view.setGraph(&m_graph);

        // Seed (a + b) -> Output, keeping editable handles to the inputs.
        m_a   = node_cast<NumberNode>(m_graph.create("Number"));
        m_b   = node_cast<NumberNode>(m_graph.create("Number"));
        auto *add = m_graph.create("Add");
        m_out = node_cast<OutputNode>(m_graph.create("Output"));
        if(m_a)  { m_a->value = 5.0f; m_a->setPos(Vec2f(-240.0f, -70.0f)); }
        if(m_b)  { m_b->value = 3.0f; m_b->setPos(Vec2f(-240.0f,  70.0f)); }
        if(add)  { add->setPos(Vec2f(-20.0f, 0.0f)); }
        if(m_out){ m_out->setPos(Vec2f(200.0f, 0.0f)); }
        if(m_a && add)   { m_graph.connect(m_a->output(0),  add->input(0)); }
        if(m_b && add)   { m_graph.connect(m_b->output(0),  add->input(1)); }
        if(add && m_out) { m_graph.connect(add->output(0),  m_out->input(0)); }

        m_keys.registerAll({
            {"view.reset", "Reset view", "Recenter the node canvas",  ImGuiMod_Ctrl | ImGuiKey_0, false, nullptr},
            {"app.quit",   "Quit",       "Close the application",     ImGuiMod_Ctrl | ImGuiKey_Q, false, nullptr},
        });
        if(m_a) { m_view.select(m_a); }   // seed the Inspector with a selected node
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

        ImGui::SetNextWindowSize(ImVec2(1080.0f, 640.0f), ImGuiCond_FirstUseEver);
        if(ImGui::Begin("imtool calc_graph example")) {
            ImGui::TextDisabled("A node graph that computes (a + b). Click a Number node, then edit its");
            ImGui::TextDisabled("value in the Inspector (auto-rendered from the node's SettingGroup).");
            ImGui::Text("result a + b = %.2f", m_out ? static_cast<double>(m_out->result) : 0.0);
            ImGui::Text("nodes %zu   edges %zu   FPS %.0f",
                        m_graph.nodeCount(), m_graph.edgeCount(),
                        static_cast<double>(ImGui::GetIO().Framerate));
            if(ImGui::Button("Reset view")) { m_view.resetView(); }
            ImGui::TextDisabled("right-click: add node | drag port->port: connect | wheel: zoom | Del: remove");
            m_view.draw();   // fills the remaining content region
        }
        ImGui::End();

        // Inspector: the selected node's settings, auto-rendered by the framework (Epic B).
        if(ImGui::Begin("Inspector")) {
            if(Node *sel = m_view.selected()) {
                ImGui::Text("%s  (#%d)", std::string(sel->name()).c_str(), sel->id());
                ImGui::Separator();
                sel->settings().draw();
            } else {
                ImGui::TextDisabled("Select a node to edit its settings.");
            }
        }
        ImGui::End();

        // The application's own settings (clear color, vsync) — observers bound into AppConfig.
        if(ImGui::Begin("App Settings")) { settings().draw(); }
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

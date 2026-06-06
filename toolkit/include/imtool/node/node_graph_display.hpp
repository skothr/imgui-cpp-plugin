#pragma once

// NodeGraphDisplay — an ImGui child-canvas view of a NodeGraph. Pan (drag empty
// space), zoom (wheel, cursor-anchored), nodes as titled boxes with port circles,
// bezier edges, drag-output-to-input to connect, right-click palette to add.
// Lifted + modernized from 19-logos nodeGraphDisplay/newNodeBar and 16-nodegraph
// nodeGraphWidget/nodeWidget. Uses imgui CORE only (no GLFW/backends).
//
// Deferred (post-beta): Houdini closer-endpoint edge editing, multi-select +
// rubber-band, node-settings auto-inspector, minimap, measured node bodies with
// live widgets, view-state persistence, keybinding integration.
//
// Coordinate model: m_scale = graph-units per screen-pixel; m_center = the graph
// point shown at the canvas center. POINT transforms apply pan+zoom; VECTOR
// transforms apply zoom only (no translation) — conflating them is a classic bug,
// so both forms are kept (deltas/sizes are vectors; positions are points).

#include <vector>

#include <imtool/common/vector.hpp>
#include <imtool/node/node.hpp>
#include <imtool/node/node_graph.hpp>
#include <imtool/node/node_registry.hpp>

namespace imtool {

class NodeGraphDisplay {
public:
    explicit NodeGraphDisplay(NodeGraph *graph = nullptr) : m_graph(graph) {}

    NodeGraphDisplay(const NodeGraphDisplay&)            = delete;
    NodeGraphDisplay& operator=(const NodeGraphDisplay&) = delete;

    void setGraph(NodeGraph *graph) { m_graph = graph; m_selected = nullptr; m_order.clear(); }
    [[nodiscard]] NodeGraph* graph() const { return m_graph; }

    // ---- view transform (canvas-relative; add the canvas origin for absolute px) ----
    [[nodiscard]] Vec2f graphToScreen (const Vec2f &g) const { return (g - m_center) / m_scale + m_size * 0.5f; }
    [[nodiscard]] Vec2f screenToGraph (const Vec2f &s) const { return (s - m_size * 0.5f) * m_scale + m_center; }
    [[nodiscard]] Vec2f graphToScreenV(const Vec2f &g) const { return g / m_scale; }   // vector: scale only
    [[nodiscard]] Vec2f screenToGraphV(const Vec2f &s) const { return s * m_scale; }

    [[nodiscard]] const Vec2f& center() const { return m_center; }
    [[nodiscard]] float        scale()  const { return m_scale;  }
    void setCenter(const Vec2f &c) { m_center = c; }
    void setScale(float s);                                   // clamped to [m_scaleMin, m_scaleMax]
    void setViewportSize(const Vec2f &sz) { m_size = sz; }    // normally set during draw(); seam for tests
    void resetView() { m_center = Vec2f(0, 0); m_scale = 1.0f; }

    // ---- selection (beta: single node) ----
    [[nodiscard]] Node* selected() const { return m_selected; }
    [[nodiscard]] Node* hovered()  const { return m_hovered;  }
    void select(Node *n);
    void deselect() { m_selected = nullptr; }

    [[nodiscard]] bool focused() const { return m_focused; }

    // The entry point: render into a child of `size` (<=0 => fill available).
    // Returns true if the graph changed (node moved, connection made/removed/added).
    bool draw(const Vec2f &size = Vec2f(-1, -1));

private:
    [[nodiscard]] Vec2f nodeBodySize(const Node *n) const;            // graph units (uses CalcTextSize)
    [[nodiscard]] Vec2f portOffset(const Node *n, bool output, int index) const;  // graph, rel. node pos
    [[nodiscard]] Vec2f portGraphPos(const Connector *c) const;
    [[nodiscard]] Connector* portAt(const Vec2f &graphPt) const;      // nearest port within hit radius
    [[nodiscard]] Node* nodeAt(const Vec2f &graphPt) const;           // top-most node under point

    void syncOrder();                                                 // keep m_order == graph nodes
    void bringToFront(Node *n);

    void drawGrid(const Vec2f &p0);
    bool drawNode(Node *n, const Vec2f &p0);                          // body + ports; true if moved
    void drawNodeEdges(const Node *n, const Vec2f &p0) const;         // output-side only (no dupes)
    void drawBezier(const Vec2f &aScreen, const Vec2f &bScreen, unsigned col, float thickness) const;
    bool handleCanvasInput(const Vec2f &p0);                          // pan/zoom/select/delete; true if changed
    bool drawContextMenu(const Vec2f &p0);                            // right-click "Add Node" palette

    NodeGraph         *m_graph   = nullptr;
    std::vector<Node*> m_order;                  // draw order (back-to-front); synced to graph
    Vec2f              m_size    {0, 0};          // last canvas size (screen px)
    Vec2f              m_center  {0, 0};
    float              m_scale    = 1.0f;
    float              m_scaleMin = 0.2f;
    float              m_scaleMax = 5.0f;
    Vec2f              m_gridSpacing {64.0f, 64.0f};

    bool        m_focused     = false;
    Node       *m_hovered     = nullptr;
    Node       *m_selected    = nullptr;
    Connector  *m_hoveredPort = nullptr;          // recomputed each frame
    Connector  *m_pendingFrom = nullptr;          // in-progress connection drag source
    bool        m_draggingNode = false;

    // appearance (graph units; scaled at draw time)
    static constexpr float kPortRadius  = 5.0f;
    static constexpr float kPortSpacing = 8.0f;
    static constexpr float kTitleHeight = 22.0f;
    static constexpr float kBodyPadding = 8.0f;
    static constexpr float kMinBodyW    = 90.0f;
    static constexpr float kEdgeTension = 0.5f;
};

}  // namespace imtool

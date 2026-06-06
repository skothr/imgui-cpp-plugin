#include <imtool/node/node_graph_display.hpp>

#include <algorithm>
#include <cmath>
#include <string>

#include <imgui.h>

#include <imtool/common/imgui_ops.hpp>

namespace imtool {

namespace {
constexpr ImU32 kColGrid     = IM_COL32(60, 60, 70, 120);
constexpr ImU32 kColTitle    = IM_COL32(60, 70, 95, 255);
constexpr ImU32 kColBody     = IM_COL32(38, 42, 52, 235);
constexpr ImU32 kColText     = IM_COL32(230, 230, 235, 255);
constexpr ImU32 kColPort     = IM_COL32(180, 190, 110, 255);
constexpr ImU32 kColPortHot  = IM_COL32(255, 240, 150, 255);
constexpr ImU32 kColEdge     = IM_COL32(180, 190, 110, 220);
constexpr ImU32 kColPending  = IM_COL32(255, 255, 255, 230);
constexpr ImU32 kColSelected = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kColHovered  = IM_COL32(150, 170, 255, 200);

[[nodiscard]] float portRow() { return 2.0f * 5.0f /*kPortRadius*/ + 8.0f /*kPortSpacing*/; }
}  // namespace

void NodeGraphDisplay::setScale(float s) { m_scale = std::clamp(s, m_scaleMin, m_scaleMax); }

void NodeGraphDisplay::select(Node *n) { m_selected = n; if(n) { bringToFront(n); } }

void NodeGraphDisplay::syncOrder() {
    if(!m_graph) { m_order.clear(); return; }
    // drop nodes no longer in the graph
    const auto &live = m_graph->nodes();
    m_order.erase(std::remove_if(m_order.begin(), m_order.end(),
                                 [&](Node *n) { return std::find(live.begin(), live.end(), n) == live.end(); }),
                  m_order.end());
    // append newly-added nodes
    for(Node *n : live) {
        if(std::find(m_order.begin(), m_order.end(), n) == m_order.end()) { m_order.push_back(n); }
    }
    if(m_selected && std::find(live.begin(), live.end(), m_selected) == live.end()) { m_selected = nullptr; }
}

void NodeGraphDisplay::bringToFront(Node *n) {
    auto it = std::find(m_order.begin(), m_order.end(), n);
    if(it != m_order.end()) { m_order.erase(it); m_order.push_back(n); }
}

Vec2f NodeGraphDisplay::nodeBodySize(const Node *n) const {
    const ImVec2 ts = ImGui::CalcTextSize(std::string(n->name()).c_str());
    const float  w  = std::max(kMinBodyW, ts.x + 2.0f * kBodyPadding);
    const int    rows = std::max<int>(1, std::max(static_cast<int>(n->inputCount()), static_cast<int>(n->outputCount())));
    const float  h  = kTitleHeight + static_cast<float>(rows) * portRow() + kBodyPadding;
    return Vec2f(w, h);
}

Vec2f NodeGraphDisplay::portOffset(const Node *n, bool output, int index) const {
    const float x = output ? nodeBodySize(n).x : 0.0f;
    const float y = kTitleHeight + kPortRadius + static_cast<float>(index) * portRow();
    return Vec2f(x, y);
}

Vec2f NodeGraphDisplay::portGraphPos(const Connector *c) const {
    return c->node()->pos() + portOffset(c->node(), c->isOutput(), c->index());
}

Connector* NodeGraphDisplay::portAt(const Vec2f &graphPt) const {
    if(!m_graph) { return nullptr; }
    for(Node *n : m_graph->nodes()) {
        for(std::size_t i = 0; i < n->inputCount(); ++i) {
            Connector *c = n->input(static_cast<int>(i));
            if((portGraphPos(c) - graphPt).length() <= kPortRadius * 1.6f) { return c; }
        }
        for(std::size_t i = 0; i < n->outputCount(); ++i) {
            Connector *c = n->output(static_cast<int>(i));
            if((portGraphPos(c) - graphPt).length() <= kPortRadius * 1.6f) { return c; }
        }
    }
    return nullptr;
}

Node* NodeGraphDisplay::nodeAt(const Vec2f &graphPt) const {
    for(auto it = m_order.rbegin(); it != m_order.rend(); ++it) {   // top-most first
        Node *n = *it;
        const Vec2f tl = n->pos();
        const Vec2f br = tl + nodeBodySize(n);
        if(graphPt.x >= tl.x && graphPt.x <= br.x && graphPt.y >= tl.y && graphPt.y <= br.y) { return n; }
    }
    return nullptr;
}

void NodeGraphDisplay::drawGrid(const Vec2f &p0) {
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const Vec2f g0 = screenToGraph(Vec2f(0, 0));
    const Vec2f g1 = screenToGraph(m_size);
    const float sx = m_gridSpacing.x, sy = m_gridSpacing.y;
    for(float gx = std::floor(g0.x / sx) * sx; gx <= g1.x; gx += sx) {
        const Vec2f a = p0 + graphToScreen(Vec2f(gx, g0.y));
        const Vec2f b = p0 + graphToScreen(Vec2f(gx, g1.y));
        dl->AddLine(toImVec(a), toImVec(b), kColGrid);
    }
    for(float gy = std::floor(g0.y / sy) * sy; gy <= g1.y; gy += sy) {
        const Vec2f a = p0 + graphToScreen(Vec2f(g0.x, gy));
        const Vec2f b = p0 + graphToScreen(Vec2f(g1.x, gy));
        dl->AddLine(toImVec(a), toImVec(b), kColGrid);
    }
}

void NodeGraphDisplay::drawBezier(const Vec2f &a, const Vec2f &b, ImU32 col, float thickness) const {
    const float dx = std::max(std::fabs(b.x - a.x) * kEdgeTension, 30.0f) / m_scale;
    const Vec2f c1 = a + Vec2f(dx, 0);
    const Vec2f c2 = b - Vec2f(dx, 0);
    ImGui::GetWindowDrawList()->AddBezierCubic(toImVec(a), toImVec(c1), toImVec(c2), toImVec(b), col, thickness);
}

void NodeGraphDisplay::drawNodeEdges(const Node *n, const Vec2f &p0) const {
    for(std::size_t i = 0; i < n->outputCount(); ++i) {
        const Connector *o = n->output(static_cast<int>(i));
        const Vec2f a = p0 + graphToScreen(portGraphPos(o));
        for(const Connector *link : o->links()) {
            const Vec2f b = p0 + graphToScreen(portGraphPos(link));
            drawBezier(a, b, kColEdge, 2.5f);
        }
    }
}

bool NodeGraphDisplay::drawNode(Node *n, const Vec2f &p0) {
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const Vec2f bodyG = nodeBodySize(n);
    const Vec2f tl = p0 + graphToScreen(n->pos());
    const Vec2f br = p0 + graphToScreen(n->pos() + bodyG);
    const float titleH = kTitleHeight / m_scale;
    const float rounding = 4.0f;

    dl->AddRectFilled(toImVec(tl), toImVec(br), kColBody, rounding);
    dl->AddRectFilled(toImVec(tl), toImVec(tl + Vec2f(br.x - tl.x, titleH)), kColTitle, rounding,
                      ImDrawFlags_RoundCornersTop);
    dl->AddText(toImVec(tl + Vec2f(kBodyPadding / m_scale, 3.0f / m_scale)), kColText,
                std::string(n->name()).c_str());

    if(n == m_selected)      { dl->AddRect(toImVec(tl), toImVec(br), kColSelected, rounding, 0, 2.0f); }
    else if(n == m_hovered)  { dl->AddRect(toImVec(tl), toImVec(br), kColHovered,  rounding, 0, 1.0f); }

    const float pr = kPortRadius / m_scale;
    const auto drawPort = [&](Connector *c) {
        const Vec2f ps = p0 + graphToScreen(portGraphPos(c));
        dl->AddCircleFilled(toImVec(ps), pr, (c == m_hoveredPort) ? kColPortHot : kColPort);
    };
    for(std::size_t i = 0; i < n->inputCount();  ++i) { drawPort(n->input(static_cast<int>(i))); }
    for(std::size_t i = 0; i < n->outputCount(); ++i) { drawPort(n->output(static_cast<int>(i))); }
    return false;
}

bool NodeGraphDisplay::handleCanvasInput(const Vec2f &p0) {
    bool changed = false;
    ImGuiIO &io = ImGui::GetIO();
    const Vec2f mouseRel   = fromImVec(ImGui::GetMousePos()) - p0;
    const Vec2f mouseGraph = screenToGraph(mouseRel);

    // zoom (wheel), anchored so the graph point under the cursor stays put
    if(ImGui::IsWindowHovered() && io.MouseWheel != 0.0f) {
        const Vec2f before = screenToGraph(mouseRel);
        setScale(m_scale * (1.0f - io.MouseWheel * 0.12f));
        const Vec2f after = screenToGraph(mouseRel);
        m_center += (before - after);
    }

    // press: port-grab > node-drag > pan
    if(ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if(m_hoveredPort)      { m_pendingFrom = m_hoveredPort; }
        else if(m_hovered)     { select(m_hovered); m_draggingNode = true; }
        else                   { select(nullptr); }
    }

    // node drag
    if(m_draggingNode && m_selected && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        const Vec2f d = fromImVec(ImGui::GetMouseDragDelta(ImGuiMouseButton_Left));
        ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
        m_selected->setPos(m_selected->pos() + screenToGraphV(d));
        changed = true;
    }

    // pan (middle-drag anywhere, or left-drag over empty space with nothing grabbed)
    const bool panBtn = ImGui::IsMouseDragging(ImGuiMouseButton_Middle) ||
                        (!m_draggingNode && !m_pendingFrom && ImGui::IsMouseDragging(ImGuiMouseButton_Left));
    if(ImGui::IsWindowHovered() && panBtn) {
        const ImGuiMouseButton btn = ImGui::IsMouseDragging(ImGuiMouseButton_Middle) ? ImGuiMouseButton_Middle
                                                                                     : ImGuiMouseButton_Left;
        const Vec2f d = fromImVec(ImGui::GetMouseDragDelta(btn));
        ImGui::ResetMouseDragDelta(btn);
        m_center -= screenToGraphV(d);
    }

    // release: finish a connection drag
    if(ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if(m_pendingFrom && m_hoveredPort && m_hoveredPort->isOutput() != m_pendingFrom->isOutput()) {
            Connector *out = m_pendingFrom->isOutput() ? m_pendingFrom : m_hoveredPort;
            Connector *in  = m_pendingFrom->isInput()  ? m_pendingFrom : m_hoveredPort;
            if(m_graph && m_graph->connect(out, in)) { changed = true; }
        }
        m_pendingFrom  = nullptr;
        m_draggingNode = false;
    }

    // remember the right-click point for new-node placement (see drawContextMenu)
    if(ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        m_menuGraphPos = mouseGraph;
    }

    // delete selected node
    if(m_focused && m_selected && ImGui::IsKeyPressed(ImGuiKey_Delete, false) && m_graph) {
        Node *victim = m_selected;
        // Null any transient pointer into the victim BEFORE remove() frees its
        // connectors — a held port-drag (m_pendingFrom) persists across frames and
        // would otherwise dangle into freed memory (use-after-free) on the next draw.
        if(m_pendingFrom && m_pendingFrom->node() == victim) { m_pendingFrom = nullptr; }
        if(m_hoveredPort && m_hoveredPort->node() == victim) { m_hoveredPort = nullptr; }
        if(m_hovered == victim) { m_hovered = nullptr; }
        m_selected = nullptr;
        m_graph->remove(victim);
        changed = true;
    }

    return changed;
}

bool NodeGraphDisplay::drawContextMenu() {
    bool added = false;
    if(ImGui::BeginPopupContextWindow("##imtool-nodegraph-ctx")) {
        if(m_graph && m_graph->registry()) {
            ImGui::TextDisabled("Add node");
            ImGui::Separator();
            for(const std::string &type : m_graph->registry()->typeNames()) {
                if(ImGui::MenuItem(type.c_str())) {
                    Node *n = m_graph->create(type);
                    if(n) {
                        // place at the graph point where the user right-clicked (the
                        // live cursor is now over the menu item, not the canvas)
                        n->setPos(m_menuGraphPos);
                        select(n);
                        added = true;
                    }
                }
            }
        } else {
            ImGui::TextDisabled("(no registry)");
        }
        ImGui::EndPopup();
    }
    return added;
}

bool NodeGraphDisplay::draw(const Vec2f &size) {
    bool changed = false;
    Vec2f canvas = size;
    if(canvas.x <= 0.0f || canvas.y <= 0.0f) { canvas = fromImVec(ImGui::GetContentRegionAvail()); }
    if(canvas.x < 1.0f) { canvas.x = 1.0f; }
    if(canvas.y < 1.0f) { canvas.y = 1.0f; }
    m_size = canvas;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    const bool vis = ImGui::BeginChild("##imtool-nodegraph", toImVec(canvas), ImGuiChildFlags_Borders,
                                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove);
    if(vis) {
        const Vec2f p0 = fromImVec(ImGui::GetCursorScreenPos());
        m_focused = ImGui::IsWindowFocused();
        const bool hov = ImGui::IsWindowHovered();
        syncOrder();
        drawGrid(p0);

        const Vec2f mouseGraph = screenToGraph(fromImVec(ImGui::GetMousePos()) - p0);
        m_hovered     = hov ? nodeAt(mouseGraph) : nullptr;
        m_hoveredPort = hov ? portAt(mouseGraph) : nullptr;

        for(Node *n : m_order) { drawNodeEdges(n, p0); }
        if(m_pendingFrom) {
            const Vec2f a = p0 + graphToScreen(portGraphPos(m_pendingFrom));
            drawBezier(a, p0 + graphToScreen(mouseGraph), kColPending, 3.0f);
        }
        for(Node *n : m_order) { drawNode(n, p0); }

        changed |= handleCanvasInput(p0);
        changed |= drawContextMenu();
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    return changed;
}

}  // namespace imtool

#include "imtest.hpp"

#include <imtool/node/node_graph_display.hpp>

using namespace imtool;

// The view's coordinate transforms are pure math (no ImGui context), so they're
// unit-testable headless. Rendering/interaction (draw()) is exercised by the
// scaffold. m_scale = graph-units per screen-pixel; m_center = graph point at
// the canvas center.
int main() {
    IMTEST_SUITE("nodeview");

    NodeGraphDisplay d(nullptr);
    d.setViewportSize(Vec2f(800, 600));
    d.setCenter(Vec2f(100, 50));
    d.setScale(2.0f);
    CHECK_NEAR(d.scale(), 2.0f, 1e-5);

    // the center graph point maps to the canvas center
    const Vec2f c = d.graphToScreen(Vec2f(100, 50));
    CHECK_NEAR(c.x, 400.0f, 1e-3);
    CHECK_NEAR(c.y, 300.0f, 1e-3);

    // point round-trip
    const Vec2f g(123.0f, -45.0f);
    const Vec2f rt = d.screenToGraph(d.graphToScreen(g));
    CHECK_NEAR(rt.x, 123.0f, 1e-3);
    CHECK_NEAR(rt.y, -45.0f, 1e-3);

    // vector round-trip (no translation)
    const Vec2f v(10.0f, 20.0f);
    const Vec2f vrt = d.screenToGraphV(d.graphToScreenV(v));
    CHECK_NEAR(vrt.x, 10.0f, 1e-3);
    CHECK_NEAR(vrt.y, 20.0f, 1e-3);

    // a 2-graph-unit offset is 1 screen pixel at scale 2
    CHECK_NEAR(d.graphToScreenV(Vec2f(2, 2)).x, 1.0f, 1e-3);
    CHECK_NEAR(d.graphToScreen(Vec2f(102, 50)).x, 401.0f, 1e-3);

    // scale clamps to [min, max]
    d.setScale(1000.0f); CHECK(d.scale() <= 5.0f);
    d.setScale(0.0001f); CHECK(d.scale() >= 0.2f);

    return imtest::report();
}

#include "imtest.hpp"

#include <vector>

#include <imtool/plot/plot.hpp>

// Headless coverage of imtool::plot (MAIN-594). draw() needs a live ImGui frame
// and is exercised by the demo + (later) an imgui_test_engine test; everything
// load-bearing is verified frame-free here:
//   - the pure data<->screen transform (the test seam, like test_nodeview), across
//     Linear / Log10 / inverted axes, X and Y, with round-trips;
//   - the Tracy-style LOD span-collapse (detail::collapseSpans) — which spans stay
//     separate, which fold into a density cluster, and how gaps break a run.

using namespace imtool;
using namespace imtool::plot;

namespace {
// identity time->pixel mapping (1 time unit == 1 px) so collapse cases read clearly
float idPx(double t) { return (float)t; }

Span span(double a, double b) { return Span{ a, b, 0, nullptr }; }
}  // namespace

int main() {
    IMTEST_SUITE("plot");

    // ── transform: continuous Linear X/Y ──
    {
        TimePlot p;
        p.setViewportRect(Vec2f(10.0f, 20.0f), Vec2f(200.0f, 100.0f));
        p.setTimeWindow(Range<double>(0.0, 100.0));          // X range lives in the link
        p.yAxis().range = Range<double>(0.0, 50.0);

        CHECK_NEAR(p.dataToScreenX(0.0),   10.0f,  1e-3);    // left edge
        CHECK_NEAR(p.dataToScreenX(100.0), 210.0f, 1e-3);    // right edge
        CHECK_NEAR(p.dataToScreenX(50.0),  110.0f, 1e-3);    // midpoint

        // Y flips: data 0 -> bottom (origin.y+size.y), data max -> top (origin.y)
        CHECK_NEAR(p.dataToScreenY(0.0),  120.0f, 1e-3);
        CHECK_NEAR(p.dataToScreenY(50.0), 20.0f,  1e-3);
        CHECK_NEAR(p.dataToScreenY(25.0), 70.0f,  1e-3);

        // round-trips
        CHECK_NEAR(p.screenToDataX(110.0f), 50.0, 1e-6);
        CHECK_NEAR(p.screenToDataY(70.0f),  25.0, 1e-6);
    }

    // ── transform: inverted X ──
    {
        TimePlot p;
        p.setViewportRect(Vec2f(0.0f, 0.0f), Vec2f(100.0f, 100.0f));
        p.setTimeWindow(Range<double>(0.0, 10.0));
        p.xAxis().inverted = true;
        CHECK_NEAR(p.dataToScreenX(0.0),  100.0f, 1e-3);     // low value -> right edge
        CHECK_NEAR(p.dataToScreenX(10.0), 0.0f,   1e-3);     // high value -> left edge
        CHECK_NEAR(p.screenToDataX(100.0f), 0.0, 1e-6);      // round-trip survives inversion
    }

    // ── transform: Log10 Y ──
    {
        TimePlot p;
        p.setViewportRect(Vec2f(0.0f, 0.0f), Vec2f(100.0f, 300.0f));
        p.yAxis().scale = AxisScale::Log10;
        p.yAxis().range = Range<double>(1.0, 1000.0);        // log10 -> [0,3]
        // value 10 -> log10 1 -> t=1/3 from bottom -> screen y = 300*(1 - 1/3) = 200
        CHECK_NEAR(p.dataToScreenY(10.0),  200.0f, 1e-2);
        CHECK_NEAR(p.dataToScreenY(100.0), 100.0f, 1e-2);    // log10 2 -> t=2/3 -> y=100
        CHECK_NEAR(p.screenToDataY(200.0f), 10.0, 1e-4);     // delinearize round-trip
    }

    // ── LOD: wide spans stay separate ──
    {
        std::vector<Span> s = { span(0, 10), span(20, 30), span(40, 50) };   // each 10px @ idPx
        std::vector<detail::SpanCluster> out;
        detail::collapseSpans([](const void *d, int i) { return static_cast<const Span *>(d)[i]; },
                              s.data(), (int)s.size(), 3.0f, idPx, out);
        CHECK(out.size() == 3);
        CHECK(!out[0].collapsed && out[0].count == 1);
        CHECK(!out[2].collapsed && out[2].count == 1);
        CHECK_NEAR(out[1].start, 20.0, 1e-9);
        CHECK_NEAR(out[1].end,   30.0, 1e-9);
    }

    // ── LOD: a sub-pixel run folds into one density cluster ──
    {
        std::vector<Span> s;
        for(int k = 0; k < 5; k++) { s.push_back(span(k * 1.0, k * 1.0 + 0.5)); }   // 0.5px wide, 0.5px gaps
        std::vector<detail::SpanCluster> out;
        detail::collapseSpans([](const void *d, int i) { return static_cast<const Span *>(d)[i]; },
                              s.data(), (int)s.size(), 3.0f, idPx, out);
        CHECK(out.size() == 1);
        CHECK(out[0].collapsed);
        CHECK(out[0].count == 5);
        CHECK(out[0].first == 0);
        CHECK_NEAR(out[0].start, 0.0, 1e-9);
        CHECK_NEAR(out[0].end,   4.5, 1e-9);   // extent spans the whole run
    }

    // ── LOD: a large gap breaks the run into two clusters ──
    {
        std::vector<Span> s = { span(0.0, 0.5), span(1.0, 1.5), span(100.0, 100.5) };
        std::vector<detail::SpanCluster> out;
        detail::collapseSpans([](const void *d, int i) { return static_cast<const Span *>(d)[i]; },
                              s.data(), (int)s.size(), 3.0f, idPx, out);
        CHECK(out.size() == 2);
        CHECK(out[0].collapsed && out[0].count == 2);     // first two fold (gap 0.5px < 3)
        CHECK_NEAR(out[0].end, 1.5, 1e-9);
        CHECK(out[1].collapsed && out[1].count == 1);     // the far span stands alone
        CHECK_NEAR(out[1].start, 100.0, 1e-9);
    }

    // ── LOD: empty / null inputs are safe ──
    {
        std::vector<detail::SpanCluster> out;
        detail::collapseSpans(nullptr, nullptr, 0, 3.0f, idPx, out);
        CHECK(out.empty());
    }

    // ── lanes register + index lookup ──
    {
        TimePlot p;
        p.setYScale(AxisScale::Categorical);
        p.addLane("a", "Agent A");
        p.addLane("b", "Agent B");
        CHECK(p.lanes().size() == 2);
        CHECK(p.laneIndex("a") == 0);
        CHECK(p.laneIndex("b") == 1);
        CHECK(p.laneIndex("missing") == -1);
    }

    // ── shared X link: two plots driven by one range ──
    {
        TimeAxisLink link;
        link.range = Range<double>(5.0, 15.0);
        TimePlot p1, p2;
        p1.setXLink(&link);
        p2.setXLink(&link);
        p1.setViewportRect(Vec2f(0, 0), Vec2f(100, 50));
        p2.setViewportRect(Vec2f(0, 0), Vec2f(100, 50));
        // both map the shared window identically
        CHECK_NEAR(p1.dataToScreenX(10.0), 50.0f, 1e-3);
        CHECK_NEAR(p2.dataToScreenX(10.0), 50.0f, 1e-3);
        // mutating the link moves both
        link.range = Range<double>(0.0, 100.0);
        CHECK_NEAR(p1.dataToScreenX(50.0), 50.0f, 1e-3);
        CHECK_NEAR(p2.dataToScreenX(50.0), 50.0f, 1e-3);
    }

    return imtest::report();
}

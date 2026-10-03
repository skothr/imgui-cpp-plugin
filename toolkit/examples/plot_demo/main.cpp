// plot_demo — a runnable imtool::plot example (issue #5).
//
// Three vertically stacked plots sharing ONE time axis (a TimeAxisLink), so a
// zoom/pan/brush on any of them moves all three together:
//   1. Resources — continuous-Y line series (synthetic CPU/RAM/token curves) fed
//      from ring buffers; demonstrates the live-scrolling time-series path.
//   2. Events    — categorical-Y scatter (X=time, Y=lane) with per-point hover
//      tooltips.
//   3. Activity  — Tracy-style swimlanes: labelled spans per lane, including a
//      dense run of sub-pixel spans that collapses into density ticks as you zoom
//      out (the LOD path).
//
// Interaction (over any plot): wheel = zoom time, drag = pan, shift+drag = brush a
// selection, double-click = reset to auto-fit. The "Live" toggle pins the window to
// the newest sample; turn it off to explore history freely.
//
// Build: configure with -DIMTOOL_BUILD_APP=ON -DIMTOOL_BUILD_EXAMPLES=ON (the smoke
// tree wires the backends with -DIMTOOL_SMOKE_WITH_APP=ON).

#include <cmath>
#include <vector>

#include <imgui.h>

#include <imtool/app/application.hpp>
#include <imtool/common/version.hpp>
#include <imtool/plot/plot.hpp>

using namespace imtool;

namespace {
// cheap deterministic jitter in [-1,1] from an integer seed (no <random> needed)
float jitter(unsigned n) {
    n = (n ^ 61u) ^ (n >> 16);
    n *= 9u; n = n ^ (n >> 4); n *= 0x27d4eb2du; n = n ^ (n >> 15);
    return (float)(n & 0xFFFF) / 32767.5f - 1.0f;
}
constexpr double kWindow = 30.0;   // live window width (seconds)
constexpr int    kMaxPts = 1200;   // ring-buffer cap per line series
}  // namespace

class PlotDemo : public Application {
public:
    PlotDemo() : Application(makeConfig()) {
        // share one X axis across all three plots
        m_resources.setXLink(&m_xlink);
        m_events.setXLink(&m_xlink);
        m_activity.setXLink(&m_xlink);

        m_resources.yAxis().label = std::string("load %");   // move-assign (dodges a g++12 -Wrestrict false positive on operator=(const char*))

        // event + activity lanes (categorical Y)
        m_events.setYScale(plot::AxisScale::Categorical);
        m_events.addLane("tool",  "tool call");
        m_events.addLane("token", "token");
        m_events.addLane("error", "error");

        m_activity.setYScale(plot::AxisScale::Categorical);
        m_activity.addLane("plan",  "planner");
        m_activity.addLane("exec",  "executor");
        m_activity.addLane("io",    "io/wait");

        m_xlink.range = Range<double>(0.0, kWindow);
    }

protected:
    bool onInit() override {
        log() << LogLevel::Info << "plot_demo ready (imtool v" << version_string() << ")"; log().flush();
        return true;
    }

    void onGui() override {
        const double now = ImGui::GetTime();
        sample(now);

        if(m_live) { m_xlink.range = Range<double>(now - kWindow, now); }

        ImGui::SetNextWindowSize(ImVec2(1100.0f, 760.0f), ImGuiCond_FirstUseEver);
        if(ImGui::Begin("imtool plot_demo")) {
            ImGui::Checkbox("Live", &m_live);
            ImGui::SameLine();
            ImGui::TextDisabled("wheel: zoom | drag: pan | shift+drag: brush | double-click: reset");
            if(const auto b = m_xlink.brush) {
                ImGui::SameLine();
                ImGui::Text("| brush: %.1f .. %.1f s", b->lower, b->upper);
            }

            const float h = (ImGui::GetContentRegionAvail().y - 16.0f) / 3.0f;

            // 1) resource lines (continuous Y)
            if(auto p = m_resources.begin(Vec2f(-1.0f, h))) {
                plot::LineStyle cpu;  cpu.color  = IM_COL32(0x4F, 0xA6, 0xF7, 0xFF); cpu.fill = true;
                plot::LineStyle ram;  ram.color  = IM_COL32(0x6C, 0xC6, 0x4F, 0xFF);
                plot::LineStyle tok;  tok.color  = IM_COL32(0xF7, 0xD0, 0x4F, 0xFF);
                p.line("cpu",    std::span<const plot::PlotPoint>(m_cpu),    cpu);
                p.line("ram",    std::span<const plot::PlotPoint>(m_ram),    ram);
                p.line("tokens", std::span<const plot::PlotPoint>(m_tokens), tok);
            }

            // 2) event scatter (categorical Y), with hover tooltips
            if(auto p = m_events.begin(Vec2f(-1.0f, h))) {
                plot::ScatterStyle st; st.marker = plot::Marker::Diamond; st.markerSize = 4.0f;
                if(auto hit = p.scatter("evts", std::span<const plot::PlotPoint>(m_evtPts), st)) {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", m_evtNames[(std::size_t)hit.index]);
                    ImGui::Text("t = %.2f s", m_evtPts[(std::size_t)hit.index].x);
                    ImGui::EndTooltip();
                }
            }

            // 3) activity swimlanes (spans + LOD collapse), with hover tooltips
            if(auto p = m_activity.begin(Vec2f(-1.0f, h))) {
                plot::SpanStyle ss;
                auto report = [&](plot::SpanHit hit) {
                    if(!hit) { return; }
                    ImGui::BeginTooltip();
                    if(hit.mergedCount > 1) { ImGui::Text("%d activities collapsed", hit.mergedCount); }
                    else                    { ImGui::Text("activity"); }
                    ImGui::Text("%.2f .. %.2f s", hit.range.lower, hit.range.upper);
                    ImGui::EndTooltip();
                };
                report(p.spans("plan", std::span<const plot::Span>(m_planSpans), ss));
                report(p.spans("exec", std::span<const plot::Span>(m_execSpans), ss));
                report(p.spans("io",   std::span<const plot::Span>(m_ioSpans),   ss));
            }
        }
        ImGui::End();
    }

private:
    static AppConfig makeConfig() {
        AppConfig c;
        c.title = "imtool plot_demo";
        c.size  = Vec2i(1280, 860);
        return c;
    }

    void pushCapped(std::vector<plot::PlotPoint> &buf, double t, double v) {
        buf.push_back(plot::PlotPoint{ t, v });
        if((int)buf.size() > kMaxPts) { buf.erase(buf.begin()); }   // ring-buffer trim (oldest-first stays contiguous)
    }

    // Append synthetic telemetry at ~20 Hz of virtual time; lazily seed the spans.
    void sample(double now) {
        if(now - m_lastSample >= 0.05) {
            m_lastSample = now;
            const unsigned k = m_tick++;
            pushCapped(m_cpu,    now, 50.0 + 40.0 * std::sin(now * 0.7) + 6.0 * jitter(k));
            pushCapped(m_ram,    now, 60.0 + 20.0 * std::sin(now * 0.2 + 1.0) + 3.0 * jitter(k + 7));
            pushCapped(m_tokens, now, 30.0 + 28.0 * std::sin(now * 1.3 + 2.0) + 5.0 * jitter(k + 13));

            // occasional events on a random-ish lane
            if((k % 17) == 0) { m_evtPts.push_back({ now, (double)(k % 3) }); m_evtNames.push_back("tool: read_file"); }
            if((k % 23) == 0) { m_evtPts.push_back({ now, 1.0 });             m_evtNames.push_back("token batch"); }
            while(m_evtPts.size() > 400) { m_evtPts.erase(m_evtPts.begin()); m_evtNames.erase(m_evtNames.begin()); }
        }
        if(!m_seededSpans && now > 0.5) { seedSpans(now); m_seededSpans = true; }
    }

    // Pre-generate activity spans, including a dense sub-pixel run to exercise LOD.
    void seedSpans(double base) {
        for(int i = 0; i < 12; i++) {
            const double s = base + i * 2.3;
            m_planSpans.push_back(plot::Span{ s, s + 1.2, 0, "plan" });
            m_execSpans.push_back(plot::Span{ s + 0.5, s + 2.0, 0, "exec" });
        }
        // io lane: a burst of 300 tiny spans (each ~5ms) -> collapses to density ticks
        for(int i = 0; i < 300; i++) {
            const double s = base + 4.0 + i * 0.02;
            m_ioSpans.push_back(plot::Span{ s, s + 0.005, 0, nullptr });
        }
    }

    // shared axis + the three plots
    plot::TimeAxisLink m_xlink;
    plot::TimePlot     m_resources{ "Resources" };
    plot::TimePlot     m_events{ "Events" };
    plot::TimePlot     m_activity{ "Activity" };

    // data
    std::vector<plot::PlotPoint> m_cpu, m_ram, m_tokens;
    std::vector<plot::PlotPoint> m_evtPts;
    std::vector<const char *>    m_evtNames;
    std::vector<plot::Span>      m_planSpans, m_execSpans, m_ioSpans;

    double   m_lastSample  = -1.0;
    unsigned m_tick        = 0;
    bool     m_live        = true;
    bool     m_seededSpans = false;
};

int main() {
    PlotDemo app;
    return app.run();
}

#pragma once

// imtool::plot — a DrawList-based plotting widget family for live telemetry and
// activity timelines. ImGui CORE only (no GLFW/backends), v1.92 docking-native,
// RAII-scoped per the toolkit's imscoped conventions. Built as a from-scratch
// alternative to ImPlot, for consumers that cannot take an ImPlot dependency.
//
// Three series kinds over one shared time axis:
//   line    — scrolling time-series (CPU/RAM/token curves; ring-buffer friendly,
//             follow-mode auto-scrolls to newest until the user pans).
//   scatter — events at X=time on a continuous OR categorical (lane) Y, with
//             per-point hover -> caller-drawn tooltip.
//   spans   — Tracy-style swimlanes: labelled time-spans on horizontal lanes,
//             with sub-pixel LOD collapse (dense runs fold into density ticks).
//
// Coordinate model mirrors NodeGraphDisplay: a pure data<->screen transform per
// axis, with a settable viewport rect, so the mapping is unit-testable without a
// live ImGui frame (see setViewportRect / dataToScreenX, exercised by test_plot).
//
// Implementation note: like settings/setting.cpp, the single plot.cpp is the only
// TU that includes <imgui.h>; this header stays ImGui-light (colors are plain
// std::uint32_t in ImU32 RGBA order) so it composes into the backend-light core.

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <imtool/common/range.hpp>     // Range<double> for axis ranges
#include <imtool/common/vector.hpp>    // Vec2f

namespace imtool::plot {

// ───────────────────────── data model ─────────────────────────

// One sample. x is the time axis (double — epoch-second precision). y is the
// value (continuous Y) or the lane index (categorical Y), also double so a single
// point type serves every series kind.
struct PlotPoint { double x = 0.0; double y = 0.0; };

// Zero-copy point accessor (ImPlot-style): no per-point heap, no std::function in
// the draw loop. `data` is the caller's raw storage; idx in [0,count). Apply ring-
// buffer wrap / stride INSIDE the getter — it sees your buffer directly.
using PointGetter = PlotPoint (*)(const void *data, int idx);

// A time-span on a lane (swimlane series). Half-open [start,end) in axis time.
struct Span {
    double        start = 0.0;
    double        end   = 0.0;
    std::uint32_t color = 0;        // ImU32 RGBA; 0 => use the lane's default
    const char   *label = nullptr;  // optional; shown when the span is wide enough
};
using SpanGetter = Span (*)(const void *data, int idx);

// ───────────────────────── axes ─────────────────────────

enum class AxisScale {
    Linear,        // continuous numeric Y (resource %, token counts)
    Log10,         // continuous, log-scaled (wide dynamic range)
    Time,          // X axis: human time tick labels (s/min/h)
    Categorical,   // Y axis: discrete lanes (scatter lanes / swimlanes)
};

// Per-axis display state. The X axis's visible range lives in TimeAxisLink (so it
// can be shared); this struct carries scale/label/fit policy/formatting. The Y
// axis additionally owns its own range here (Y is never shared across plots).
struct PlotAxis {
    Range<double> range    {0.0, 1.0};   // Y uses this; X reads/writes the link
    AxisScale     scale    = AxisScale::Linear;
    bool          autoFit  = true;        // refit from data until the user interacts
    bool          inverted = false;
    std::string   label;
    // Format a tick value -> label (bytes->"1.2 GB", epoch->"12:30:05"). Null =>
    // default numeric (Y) / time (X) formatting.
    std::function<std::string(double value)> formatTick;
};

// Discrete row for a Categorical Y axis (scatter lanes / swimlanes). Lanes stack
// top-to-bottom in registration order.
struct Lane {
    std::string   id;            // stable key (ID-stack hygiene + hit reporting)
    std::string   label;         // row header text
    std::uint32_t color = 0;     // default span/point tint for this lane (0 => theme)
};

// Shared time-range for vertically stacked plots. Give several TimePlots the SAME
// TimeAxisLink* (setXLink) and zoom / pan / brush on any one drives them all —
// requirement 4 (shared-axis interaction) in one object. Owned by the caller, or
// by a TimePlot's internal default when unshared.
struct TimeAxisLink {
    Range<double>                range  {0.0, 1.0};   // visible time window
    bool                         follow = false;      // auto-scroll to newest sample
    double                       followWindow = 0.0;  // window width when following (s)
    std::optional<Range<double>> brush;               // committed drag-selection, if any
};

// ───────────────────────── styling ─────────────────────────

enum class Marker { None, Circle, Square, Diamond, Triangle, Cross };

struct LineStyle {
    std::uint32_t color      = 0;        // 0 => auto-assigned from the palette
    float         thickness  = 1.5f;
    bool          fill       = false;    // shade to the Y baseline
    std::uint32_t fillColor  = 0;        // 0 => `color` at reduced alpha
    Marker        marker     = Marker::None;
    float         markerSize = 3.0f;
};

struct ScatterStyle {
    std::uint32_t color         = 0;
    Marker        marker        = Marker::Circle;
    float         markerSize     = 4.0f;
    float         hoverRadiusPx  = 8.0f; // nearest-point pick radius for tooltips
};

struct SpanStyle {
    float laneHeightPx = 22.0f;          // row height (px)
    float minSpanPx    = 3.0f;           // Tracy LOD: narrower runs collapse
    bool  showLabels   = true;           // draw span labels when wide enough
    float roundingPx   = 2.0f;
};

// ───────────────────────── hit results ─────────────────────────

// Returned by scatter(): index of the hovered point (-1 if none) so the caller
// renders its own tooltip — keeps the widget content-agnostic + allocation-free.
struct PointHit {
    int index = -1;
    [[nodiscard]] explicit operator bool() const { return index >= 0; }
};

// Returned by spans(): a single hovered span (index>=0, mergedCount==1) OR a
// collapsed LOD cluster (mergedCount>1, range covers the merged run).
struct SpanHit {
    int           index       = -1;     // first span in the hit (or the lone span)
    int           mergedCount = 0;      // >1 => a sub-pixel collapsed cluster
    Range<double> range       {0.0, 0.0};
    [[nodiscard]] explicit operator bool() const { return index >= 0; }
};

// ───────────────────────── LOD (Tracy span collapse) ─────────────────────────

namespace detail {

// One draw item from the span LOD pass: either a single span (count==1,
// collapsed==false) or a sub-pixel cluster folded into a density tick
// (collapsed==true, count == number of spans merged). [start,end] is the data-time
// extent covered.
struct SpanCluster {
    int    first     = 0;       // index of the first span in this item
    int    count     = 0;       // spans represented (1 => lone span)
    double start     = 0.0;
    double end       = 0.0;
    bool   collapsed = false;   // true => a sub-pixel LOD cluster
};

// Pure Tracy-style LOD pass: walks spans in start order and folds runs whose
// pixel gaps stay under `minSpanPx` into one cluster. `toPx` maps a data time to a
// screen x — abstracted so this is unit-testable without a live ImGui frame. Fills
// `out` (cleared first); reuse the vector across calls to avoid per-frame alloc.
void collapseSpans(SpanGetter get, const void *data, int count, float minSpanPx,
                   const std::function<float(double)> &toPx, std::vector<SpanCluster> &out);

}  // namespace detail

// ───────────────────────── per-frame submission guard ─────────────────────────

class TimePlot;

// RAII frame guard (conditional-end, per imscoped rules): EndChild() + the plot's
// endFrame() run in the destructor only when begin() actually started the plot.
// Submit series via its methods; check `if (frame)` before submitting.
//
//   if (auto p = plot.begin({-1, 160})) {
//       p.line("cpu", cpuGetter, samples.data(), (int)samples.size(), cpuStyle);
//       if (auto hit = p.scatter("evt", evtGetter, evts.data(), n, evtStyle)) {
//           ImGui::BeginTooltip(); ImGui::Text("%s", evts[hit.index].name); ImGui::EndTooltip();
//       }
//   }   // ~PlotFrame -> endFrame()
class PlotFrame {
public:
    ~PlotFrame();
    PlotFrame(PlotFrame &&other) noexcept;
    PlotFrame &operator=(PlotFrame &&)      = delete;
    PlotFrame(const PlotFrame &)            = delete;
    PlotFrame &operator=(const PlotFrame &) = delete;

    [[nodiscard]] explicit operator bool() const { return m_started; }

    // Continuous-Y series. `id` scopes the ID stack + selects the palette color.
    void     line   (const char *id, PointGetter get, const void *data, int count, const LineStyle &style = {});
    void     line   (const char *id, std::span<const PlotPoint> pts,    const LineStyle &style = {});
    PointHit scatter(const char *id, PointGetter get, const void *data, int count, const ScatterStyle &style = {});
    PointHit scatter(const char *id, std::span<const PlotPoint> pts,    const ScatterStyle &style = {});

    // Swimlane series — requires a Categorical Y. `laneId` must match a registered
    // Lane (TimePlot::addLane). The span's y is the lane, not PlotPoint.y.
    SpanHit  spans  (const char *laneId, SpanGetter get, const void *data, int count, const SpanStyle &style = {});
    SpanHit  spans  (const char *laneId, std::span<const Span> items,      const SpanStyle &style = {});

private:
    friend class TimePlot;
    PlotFrame(TimePlot *owner, bool started) : m_owner(owner), m_started(started) {}
    TimePlot *m_owner   = nullptr;
    bool      m_started = false;
};

// ───────────────────────── the widget ─────────────────────────

class TimePlot {
public:
    // m_x defaults to a Time-scaled axis; set in the body (not a designated member
    // initializer) to avoid clang's -Wmissing-designated-field-initializers on the
    // other PlotAxis fields.
    TimePlot() { m_x.scale = AxisScale::Time; }
    explicit TimePlot(std::string title) : m_title(std::move(title)) { m_x.scale = AxisScale::Time; }

    TimePlot(const TimePlot &)            = delete;
    TimePlot &operator=(const TimePlot &) = delete;

    // Begin a frame: opens a child of `size` (<=0 => fill available), draws axes/
    // grid, installs the transform, and handles pan/zoom/brush input. Submit series
    // on the returned guard; the guard's destructor ends the plot.
    [[nodiscard]] PlotFrame begin(const Vec2f &size = Vec2f(-1, -1));

    // ---- axes ----
    [[nodiscard]] PlotAxis &xAxis() { return m_x; }
    [[nodiscard]] PlotAxis &yAxis() { return m_y; }
    void setYScale(AxisScale s) { m_y.scale = s; }    // Categorical enables lanes

    // ---- categorical lanes (scatter lanes / swimlanes) ----
    Lane &addLane(std::string id, std::string label, std::uint32_t color = 0);
    [[nodiscard]] const std::vector<Lane> &lanes() const { return m_lanes; }
    [[nodiscard]] int laneIndex(const char *id) const;   // -1 if unknown
    void clearLanes() { m_lanes.clear(); }

    // ---- shared X (stacked plots) ----
    // Default: each plot owns an internal TimeAxisLink. setXLink points this plot at
    // a caller-owned link so multiple plots share zoom/pan/brush. Passing nullptr
    // reverts to the internal link.
    void setXLink(TimeAxisLink *link) { m_xlink = link ? link : &m_ownXLink; }
    [[nodiscard]] TimeAxisLink &xLink() { return *m_xlink; }

    // ---- view ops ----
    void setTimeWindow(Range<double> r) { m_xlink->range = r; m_xlink->follow = false; }
    void setFollow(bool on, double window_seconds = 0.0);   // auto-scroll-to-newest
    void resetView();
    [[nodiscard]] std::optional<Range<double>> brush() const { return m_xlink->brush; }
    void clearBrush() { m_xlink->brush.reset(); }

    // ---- transform (pure; the headless test seam, à la NodeGraphDisplay) ----
    void  setViewportRect(const Vec2f &origin, const Vec2f &size);   // normally set in begin()
    [[nodiscard]] Vec2f  viewportOrigin() const { return m_origin; }
    [[nodiscard]] Vec2f  viewportSize()   const { return m_size; }
    [[nodiscard]] float  dataToScreenX(double x) const;
    [[nodiscard]] float  dataToScreenY(double y) const;
    [[nodiscard]] double screenToDataX(float px) const;
    [[nodiscard]] double screenToDataY(float py) const;

private:
    friend class PlotFrame;

    // series renderers (called by PlotFrame; defined in plot.cpp)
    void     drawLine   (const char *id, PointGetter get, const void *data, int count, const LineStyle &style);
    PointHit drawScatter(const char *id, PointGetter get, const void *data, int count, const ScatterStyle &style);
    SpanHit  drawSpans  (const char *laneId, SpanGetter get, const void *data, int count, const SpanStyle &style);
    void     handleInput(bool active);   // pan/zoom/brush/reset (mutates the visible range up front)
    void     drawChrome();               // plot frame + grid + tick labels + lane headers
    void     endFrame();                 // pop clip + deferred fit/follow + child close

    // accumulators reset in begin(), updated by series, consumed in endFrame() for
    // deferred auto-fit / follow (avoids the transform vs data-extent chicken-egg).
    void accumulateX(double x);
    void accumulateY(double y);

    std::string       m_title;
    PlotAxis          m_x;   // scale set to Time in the constructor body
    PlotAxis          m_y;
    std::vector<Lane> m_lanes;

    TimeAxisLink      m_ownXLink;                 // used when unshared
    TimeAxisLink     *m_xlink = &m_ownXLink;

    Vec2f m_origin {0, 0};   // plot-area top-left (screen px)
    Vec2f m_size   {0, 0};   // plot-area size (screen px)

    // per-frame data extent (for deferred fit/follow)
    bool   m_haveDataX = false, m_haveDataY = false;
    double m_dataXlo = 0.0, m_dataXhi = 0.0;
    double m_dataYlo = 0.0, m_dataYhi = 0.0;

    int    m_paletteCursor = 0;       // auto color assignment within a frame
    bool   m_inFrame       = false;   // between begin() and endFrame()
    bool   m_frameVisible  = false;   // BeginChild returned true this frame
    bool   m_areaHovered   = false;   // mouse over the plot area (gates hover/tooltips)

    // transient brush (shift-drag time selection) state
    bool   m_brushing      = false;
    double m_brushAnchorX  = 0.0;
};

}  // namespace imtool::plot

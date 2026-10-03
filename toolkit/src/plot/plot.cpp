// imtool::plot implementation — the single TU that includes <imgui.h> (the header
// stays backend-light, like settings/setting.cpp). Renders via ImDrawList only.
//
// Layout of a frame: a bordered child window split into a left gutter (Y labels /
// lane headers), a bottom gutter (X time ticks), an optional title row, and the
// plot area. The plot area is the transform viewport. Interaction (left-drag pan,
// wheel zoom, shift-drag brush, double-click reset) mutates the visible range up
// front, so series drawn this frame reflect it immediately; auto-fit / follow are
// applied at endFrame() for the NEXT frame (deferred — this avoids the transform
// vs data-extent chicken-and-egg, since the extent isn't known until series run).

#include <imtool/plot/plot.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>   // ImClamp (internal but stable; used widely by extensions)

#include <imtool/common/imgui_ops.hpp>

namespace imtool::plot {
namespace {

constexpr float kLeftGutter   = 52.0f;   // Y labels / lane headers
constexpr float kBottomGutter = 20.0f;   // X tick labels
constexpr float kPadTop       = 4.0f;
constexpr float kPadRight     = 8.0f;

// Default categorical color palette (ImU32 RGBA). Used when a style/lane color is 0.
const ImU32 kPalette[] = {
    IM_COL32(0x4F, 0xA6, 0xF7, 0xFF), IM_COL32(0xF7, 0x8C, 0x4F, 0xFF),
    IM_COL32(0x6C, 0xC6, 0x4F, 0xFF), IM_COL32(0xE0, 0x5A, 0x7A, 0xFF),
    IM_COL32(0xB4, 0x8E, 0xF7, 0xFF), IM_COL32(0x4F, 0xD0, 0xC6, 0xFF),
    IM_COL32(0xF7, 0xD0, 0x4F, 0xFF), IM_COL32(0x9A, 0x9A, 0xA8, 0xFF),
};
constexpr int kPaletteCount = (int)(sizeof(kPalette) / sizeof(kPalette[0]));

[[nodiscard]] ImU32 withAlpha(ImU32 c, float a) {
    const ImU32 newA = (ImU32)(ImClamp(a, 0.0f, 1.0f) * 255.0f + 0.5f);
    return (c & ~IM_COL32_A_MASK) | (newA << IM_COL32_A_SHIFT);
}

// Map a value through the axis scale into a linearized coordinate (and back), so
// Log10 axes share the same affine viewport math as Linear/Time.
[[nodiscard]] double linearize(double v, AxisScale s) {
    if(s == AxisScale::Log10) { return std::log10(std::max(v, 1e-12)); }
    return v;
}
[[nodiscard]] double delinearize(double v, AxisScale s) {
    if(s == AxisScale::Log10) { return std::pow(10.0, v); }
    return v;
}

// "Nice number" tick step for a span aiming for ~target ticks (1/2/5 * 10^k).
[[nodiscard]] double niceStep(double span, int target) {
    if(span <= 0.0 || target <= 0) { return 1.0; }
    const double raw  = span / target;
    const double mag  = std::pow(10.0, std::floor(std::log10(raw)));
    const double norm = raw / mag;
    double nice = 10.0;
    if(norm < 1.5)      { nice = 1.0; }
    else if(norm < 3.0) { nice = 2.0; }
    else if(norm < 7.0) { nice = 5.0; }
    return nice * mag;
}

// Default X time-tick label (HH:MM:SS when the span is wide, else MM:SS). Treats
// the value as epoch seconds; callers override via PlotAxis::formatTick.
[[nodiscard]] std::string formatTimeTick(double value, double spanSeconds) {
    const std::time_t t = (std::time_t)value;
    std::tm tmv{};
#if defined(_WIN32)
    gmtime_s(&tmv, &t);
#else
    gmtime_r(&t, &tmv);
#endif
    char buf[32];
    if(spanSeconds >= 600.0) { std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tmv.tm_hour, tmv.tm_min, tmv.tm_sec); }
    else                     { std::snprintf(buf, sizeof(buf), "%02d:%02d", tmv.tm_min, tmv.tm_sec); }
    return buf;
}

[[nodiscard]] std::string formatNumber(double v) {
    char buf[32];
    const double a = std::fabs(v);
    if(a != 0.0 && (a < 1e-3 || a >= 1e6)) { std::snprintf(buf, sizeof(buf), "%.2e", v); }
    else                                   { std::snprintf(buf, sizeof(buf), "%.4g", v); }
    return buf;
}

void drawMarker(ImDrawList *dl, Marker m, ImVec2 p, float r, ImU32 col) {
    switch(m) {
        case Marker::None:     break;
        case Marker::Circle:   dl->AddCircleFilled(p, r, col, 12); break;
        case Marker::Square:   dl->AddRectFilled(ImVec2(p.x - r, p.y - r), ImVec2(p.x + r, p.y + r), col); break;
        case Marker::Diamond:  dl->AddQuadFilled(ImVec2(p.x, p.y - r), ImVec2(p.x + r, p.y),
                                                 ImVec2(p.x, p.y + r), ImVec2(p.x - r, p.y), col); break;
        case Marker::Triangle: dl->AddTriangleFilled(ImVec2(p.x, p.y - r), ImVec2(p.x + r, p.y + r),
                                                     ImVec2(p.x - r, p.y + r), col); break;
        case Marker::Cross:    dl->AddLine(ImVec2(p.x - r, p.y - r), ImVec2(p.x + r, p.y + r), col, 1.5f);
                               dl->AddLine(ImVec2(p.x - r, p.y + r), ImVec2(p.x + r, p.y - r), col, 1.5f); break;
    }
}

// [y0,y1] screen band for lane `idx` of `n` lanes within [originY, originY+sizeY].
void laneBand(float originY, float sizeY, int idx, int n, float &y0, float &y1) {
    const float h = (n > 0) ? sizeY / (float)n : sizeY;
    y0 = originY + h * (float)idx;
    y1 = y0 + h;
}

// reusable scratch for line polylines (avoids per-call heap churn on the hot path)
std::vector<ImVec2> g_polyScratch;

}  // namespace

// ───────────────────────── LOD: pure span collapse (Tracy) ─────────────────────

namespace detail {

void collapseSpans(SpanGetter get, const void *data, int count, float minSpanPx,
                   const std::function<float(double)> &toPx, std::vector<SpanCluster> &out) {
    out.clear();
    if(!get || count <= 0 || !toPx) { return; }

    int i = 0;
    while(i < count) {
        const Span s  = get(data, i);
        float      xa = toPx(s.start), xb = toPx(s.end);
        if(xb < xa) { std::swap(xa, xb); }

        if((xb - xa) >= minSpanPx) {
            out.push_back(SpanCluster{ i, 1, s.start, s.end, false });   // wide enough to draw on its own
            i++;
            continue;
        }
        // sub-pixel: fold adjacent spans until a gap >= minSpanPx ends the run
        const int first = i;
        double    cs = s.start, ce = s.end;
        float     crx = xb;
        int       cnt = 1, j = i + 1;
        while(j < count) {
            const Span sj  = get(data, j);
            float      jx  = toPx(sj.start), jxe = toPx(sj.end);
            if(jxe < jx) { std::swap(jx, jxe); }
            if(jx - crx > minSpanPx) { break; }
            ce  = std::max(ce, sj.end);
            crx = std::max(crx, jxe);
            cnt++; j++;
        }
        out.push_back(SpanCluster{ first, cnt, cs, ce, true });
        i = j;
    }
}

}  // namespace detail

// ───────────────────────── PlotFrame ─────────────────────────

PlotFrame::PlotFrame(PlotFrame &&other) noexcept : m_owner(other.m_owner), m_started(other.m_started) {
    other.m_owner   = nullptr;   // moved-from must not end the plot
    other.m_started = false;
}

PlotFrame::~PlotFrame() {
    if(m_owner) { m_owner->endFrame(); }
}

void PlotFrame::line(const char *id, PointGetter get, const void *data, int count, const LineStyle &style) {
    if(m_owner) { m_owner->drawLine(id, get, data, count, style); }
}
void PlotFrame::line(const char *id, std::span<const PlotPoint> pts, const LineStyle &style) {
    if(!m_owner) { return; }
    m_owner->drawLine(id, [](const void *d, int i) { return static_cast<const PlotPoint *>(d)[i]; },
                      pts.data(), (int)pts.size(), style);
}
PointHit PlotFrame::scatter(const char *id, PointGetter get, const void *data, int count, const ScatterStyle &style) {
    return m_owner ? m_owner->drawScatter(id, get, data, count, style) : PointHit{};
}
PointHit PlotFrame::scatter(const char *id, std::span<const PlotPoint> pts, const ScatterStyle &style) {
    if(!m_owner) { return {}; }
    return m_owner->drawScatter(id, [](const void *d, int i) { return static_cast<const PlotPoint *>(d)[i]; },
                                pts.data(), (int)pts.size(), style);
}
SpanHit PlotFrame::spans(const char *laneId, SpanGetter get, const void *data, int count, const SpanStyle &style) {
    return m_owner ? m_owner->drawSpans(laneId, get, data, count, style) : SpanHit{};
}
SpanHit PlotFrame::spans(const char *laneId, std::span<const Span> items, const SpanStyle &style) {
    if(!m_owner) { return {}; }
    return m_owner->drawSpans(laneId, [](const void *d, int i) { return static_cast<const Span *>(d)[i]; },
                              items.data(), (int)items.size(), style);
}

// ───────────────────────── TimePlot: lanes + transform ─────────────────────────

Lane &TimePlot::addLane(std::string id, std::string label, std::uint32_t color) {
    m_lanes.push_back(Lane{ std::move(id), std::move(label), color });
    return m_lanes.back();
}
int TimePlot::laneIndex(const char *id) const {
    for(int i = 0; i < (int)m_lanes.size(); i++) { if(m_lanes[(std::size_t)i].id == id) { return i; } }
    return -1;
}

void TimePlot::setViewportRect(const Vec2f &origin, const Vec2f &size) { m_origin = origin; m_size = size; }

float TimePlot::dataToScreenX(double x) const {
    const double lo = linearize(m_xlink->range.lower, m_x.scale);
    const double hi = linearize(m_xlink->range.upper, m_x.scale);
    const double sp = hi - lo;
    const double t  = (sp != 0.0) ? (linearize(x, m_x.scale) - lo) / sp : 0.0;
    const double tt = m_x.inverted ? (1.0 - t) : t;
    return m_origin.x + (float)(tt * (double)m_size.x);
}
float TimePlot::dataToScreenY(double y) const {
    const double lo = linearize(m_y.range.lower, m_y.scale);
    const double hi = linearize(m_y.range.upper, m_y.scale);
    const double sp = hi - lo;
    const double t  = (sp != 0.0) ? (linearize(y, m_y.scale) - lo) / sp : 0.0;
    // screen Y grows downward, data Y grows up -> flip (unless inverted)
    const double tt = m_y.inverted ? t : (1.0 - t);
    return m_origin.y + (float)(tt * (double)m_size.y);
}
double TimePlot::screenToDataX(float px) const {
    const double lo = linearize(m_xlink->range.lower, m_x.scale);
    const double hi = linearize(m_xlink->range.upper, m_x.scale);
    const double t  = (m_size.x != 0.0f) ? (double)(px - m_origin.x) / (double)m_size.x : 0.0;
    const double tt = m_x.inverted ? (1.0 - t) : t;
    return delinearize(lo + tt * (hi - lo), m_x.scale);
}
double TimePlot::screenToDataY(float py) const {
    const double lo = linearize(m_y.range.lower, m_y.scale);
    const double hi = linearize(m_y.range.upper, m_y.scale);
    const double t  = (m_size.y != 0.0f) ? (double)(py - m_origin.y) / (double)m_size.y : 0.0;
    const double tt = m_y.inverted ? t : (1.0 - t);
    return delinearize(lo + tt * (hi - lo), m_y.scale);
}

void TimePlot::accumulateX(double x) {
    if(!std::isfinite(x)) { return; }
    if(!m_haveDataX) { m_dataXlo = m_dataXhi = x; m_haveDataX = true; }
    else             { m_dataXlo = std::min(m_dataXlo, x); m_dataXhi = std::max(m_dataXhi, x); }
}
void TimePlot::accumulateY(double y) {
    if(!std::isfinite(y)) { return; }
    if(!m_haveDataY) { m_dataYlo = m_dataYhi = y; m_haveDataY = true; }
    else             { m_dataYlo = std::min(m_dataYlo, y); m_dataYhi = std::max(m_dataYhi, y); }
}

void TimePlot::setFollow(bool on, double window_seconds) {
    m_xlink->follow = on;
    if(window_seconds > 0.0) { m_xlink->followWindow = window_seconds; }
    if(on) { m_x.autoFit = false; }
}
void TimePlot::resetView() {
    m_x.autoFit = true;
    m_y.autoFit = true;
    m_xlink->follow = false;
    m_xlink->brush.reset();
}

// ───────────────────────── TimePlot: begin / chrome / input ─────────────────────────

PlotFrame TimePlot::begin(const Vec2f &size) {
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 sz((size.x > 0.0f) ? size.x : avail.x, (size.y > 0.0f) ? size.y : avail.y);
    sz.x = std::max(sz.x, 32.0f);
    sz.y = std::max(sz.y, 32.0f);

    ImGui::PushID(this);   // distinct ID per plot instance (multiple plots in one window)
    const bool visible = ImGui::BeginChild("##imtool_plot", sz, ImGuiChildFlags_Borders,
                                           ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    m_inFrame       = true;
    m_frameVisible  = visible;
    m_paletteCursor = 0;
    m_haveDataX = m_haveDataY = false;
    m_areaHovered = false;

    if(!visible) { return PlotFrame(this, false); }

    ImDrawList  *dl     = ImGui::GetWindowDrawList();
    const ImVec2 winPos = ImGui::GetWindowPos();
    const ImVec2 winSz  = ImGui::GetWindowSize();
    const float  titleH = m_title.empty() ? 0.0f : (ImGui::GetTextLineHeight() + 2.0f);

    const ImVec2 origin(winPos.x + kLeftGutter, winPos.y + kPadTop + titleH);
    const ImVec2 area(std::max(1.0f, winSz.x - kLeftGutter - kPadRight),
                      std::max(1.0f, winSz.y - kPadTop - titleH - kBottomGutter));
    setViewportRect(fromImVec(origin), Vec2f(area.x, area.y));

    if(!m_title.empty()) {
        dl->AddText(ImVec2(winPos.x + 6.0f, winPos.y + 2.0f),
                    ImGui::GetColorU32(ImGuiCol_Text), m_title.c_str());
    }

    // Input capture over the plot area only (gutters stay non-interactive).
    ImGui::SetCursorScreenPos(origin);
    ImGui::InvisibleButton("##area", area, ImGuiButtonFlags_MouseButtonLeft);
    m_areaHovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    handleInput(active);

    drawChrome();

    // bracket caller-submitted series with a clip to the plot area (gutters/labels
    // were drawn unclipped above); popped in endFrame().
    dl->PushClipRect(origin, ImVec2(origin.x + area.x, origin.y + area.y), true);
    return PlotFrame(this, true);
}

void TimePlot::handleInput(bool active) {
    ImGuiIO &io = ImGui::GetIO();
    const bool shift = io.KeyShift;

    // double-click: reset to auto-fit
    if(m_areaHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) { resetView(); return; }

    // shift-drag: brush a time selection
    if(shift && m_areaHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        m_brushing     = true;
        m_brushAnchorX = screenToDataX(io.MousePos.x);
    }
    if(m_brushing) {
        if(ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            const double cur = screenToDataX(io.MousePos.x);
            const double a = std::min(m_brushAnchorX, cur), b = std::max(m_brushAnchorX, cur);
            // live selection overlay
            ImDrawList *dl = ImGui::GetWindowDrawList();
            const float xa = dataToScreenX(a), xb = dataToScreenX(b);
            dl->AddRectFilled(ImVec2(xa, m_origin.y), ImVec2(xb, m_origin.y + m_size.y), withAlpha(IM_COL32(0x4F,0xA6,0xF7,0xFF), 0.18f));
            dl->AddLine(ImVec2(xa, m_origin.y), ImVec2(xa, m_origin.y + m_size.y), withAlpha(IM_COL32_WHITE, 0.5f), 1.0f);
            dl->AddLine(ImVec2(xb, m_origin.y), ImVec2(xb, m_origin.y + m_size.y), withAlpha(IM_COL32_WHITE, 0.5f), 1.0f);
        } else {
            const double cur = screenToDataX(io.MousePos.x);
            const double a = std::min(m_brushAnchorX, cur), b = std::max(m_brushAnchorX, cur);
            if(b > a) { m_xlink->brush = Range<double>(a, b); }
            m_brushing = false;
        }
        return;   // brushing suppresses pan
    }

    // left-drag: pan X (in linearized space so Log10 pans correctly)
    if(active && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
        const float dx = io.MouseDelta.x;
        if(dx != 0.0f && m_size.x > 0.0f) {
            double lo = linearize(m_xlink->range.lower, m_x.scale);
            double hi = linearize(m_xlink->range.upper, m_x.scale);
            const double shiftAmt = (double)(-dx) / (double)m_size.x * (hi - lo);
            lo += shiftAmt; hi += shiftAmt;
            m_xlink->range = Range<double>(delinearize(lo, m_x.scale), delinearize(hi, m_x.scale));
            m_x.autoFit = false; m_xlink->follow = false;
        }
    }

    // wheel: zoom X anchored at the cursor
    if(m_areaHovered && io.MouseWheel != 0.0f) {
        const double anchor = linearize(screenToDataX(io.MousePos.x), m_x.scale);
        const double factor = std::pow(1.1, (double)-io.MouseWheel);
        double lo = linearize(m_xlink->range.lower, m_x.scale);
        double hi = linearize(m_xlink->range.upper, m_x.scale);
        lo = anchor + (lo - anchor) * factor;
        hi = anchor + (hi - anchor) * factor;
        if(hi > lo) {
            m_xlink->range = Range<double>(delinearize(lo, m_x.scale), delinearize(hi, m_x.scale));
            m_x.autoFit = false; m_xlink->follow = false;
        }
    }
}

void TimePlot::drawChrome() {
    ImDrawList  *dl   = ImGui::GetWindowDrawList();
    const ImVec2 o    = toImVec(m_origin);
    const ImVec2 o1(o.x + m_size.x, o.y + m_size.y);
    const ImU32  grid = ImGui::GetColorU32(ImGuiCol_Border, 0.5f);
    const ImU32  axis = ImGui::GetColorU32(ImGuiCol_Border);
    const ImU32  txt  = ImGui::GetColorU32(ImGuiCol_TextDisabled);

    // plot-area frame
    dl->AddRect(o, o1, axis);

    // axis labels at the top corners (inside the plot, clear of the bottom ticks):
    // Y top-left, X top-right.
    if(!m_y.label.empty()) { dl->AddText(ImVec2(o.x + 4.0f, o.y + 2.0f), txt, m_y.label.c_str()); }
    if(!m_x.label.empty()) {
        const ImVec2 ts = ImGui::CalcTextSize(m_x.label.c_str());
        dl->AddText(ImVec2(o1.x - ts.x - 4.0f, o.y + 2.0f), txt, m_x.label.c_str());
    }

    const double xlo = m_xlink->range.lower, xhi = m_xlink->range.upper, xspan = xhi - xlo;

    // ---- X grid + tick labels ----
    if(xspan > 0.0) {
        const double step  = niceStep(xspan, 6);
        const double first = std::ceil(xlo / step) * step;
        for(double t = first; t <= xhi + step * 0.5; t += step) {
            const float x = dataToScreenX(t);
            if(x < o.x - 1.0f || x > o1.x + 1.0f) { continue; }
            dl->AddLine(ImVec2(x, o.y), ImVec2(x, o1.y), grid);
            const std::string lab = m_x.formatTick ? m_x.formatTick(t)
                                  : (m_x.scale == AxisScale::Time ? formatTimeTick(t, xspan) : formatNumber(t));
            const ImVec2 ts = ImGui::CalcTextSize(lab.c_str());
            dl->AddText(ImVec2(x - ts.x * 0.5f, o1.y + 3.0f), txt, lab.c_str());
        }
    }

    // ---- Y: lanes (categorical) or value grid (continuous) ----
    if(m_y.scale == AxisScale::Categorical) {
        const int n = (int)m_lanes.size();
        for(int i = 0; i < n; i++) {
            float y0, y1; laneBand(o.y, m_size.y, i, n, y0, y1);
            if(i & 1) { dl->AddRectFilled(ImVec2(o.x, y0), ImVec2(o1.x, y1), ImGui::GetColorU32(ImGuiCol_FrameBg, 0.4f)); }
            dl->AddLine(ImVec2(o.x, y1), ImVec2(o1.x, y1), grid);
            const Lane &ln = m_lanes[(std::size_t)i];
            const ImVec2 ts = ImGui::CalcTextSize(ln.label.c_str());
            dl->AddText(ImVec2(o.x - 6.0f - ts.x, (y0 + y1) * 0.5f - ts.y * 0.5f), txt, ln.label.c_str());
        }
    } else {
        const double ylo = m_y.range.lower, yhi = m_y.range.upper, yspan = yhi - ylo;
        if(yspan > 0.0) {
            const double step  = niceStep(yspan, 5);
            const double first = std::ceil(ylo / step) * step;
            for(double t = first; t <= yhi + step * 0.5; t += step) {
                const float y = dataToScreenY(t);
                if(y < o.y - 1.0f || y > o1.y + 1.0f) { continue; }
                dl->AddLine(ImVec2(o.x, y), ImVec2(o1.x, y), grid);
                const std::string lab = m_y.formatTick ? m_y.formatTick(t) : formatNumber(t);
                const ImVec2 ts = ImGui::CalcTextSize(lab.c_str());
                dl->AddText(ImVec2(o.x - 6.0f - ts.x, y - ts.y * 0.5f), txt, lab.c_str());
            }
        }
    }
}

// ───────────────────────── TimePlot: series renderers ─────────────────────────

void TimePlot::drawLine(const char * /*id*/, PointGetter get, const void *data, int count, const LineStyle &style) {
    if(!m_frameVisible || !get || count <= 0) { return; }
    ImDrawList *dl  = ImGui::GetWindowDrawList();
    const ImU32 col = style.color ? style.color : kPalette[(m_paletteCursor++) % kPaletteCount];

    std::vector<ImVec2> &pts = g_polyScratch;
    pts.clear();
    pts.reserve((std::size_t)count);
    for(int i = 0; i < count; i++) {
        const PlotPoint p = get(data, i);
        accumulateX(p.x); accumulateY(p.y);
        pts.push_back(ImVec2(dataToScreenX(p.x), dataToScreenY(p.y)));
    }
    if(style.fill && pts.size() >= 2) {
        const float baseY = dataToScreenY(std::max(0.0, m_y.range.lower));
        const ImU32 fcol  = style.fillColor ? style.fillColor : withAlpha(col, 0.16f);
        for(std::size_t i = 0; i + 1 < pts.size(); i++) {
            dl->AddQuadFilled(pts[i], pts[i + 1], ImVec2(pts[i + 1].x, baseY), ImVec2(pts[i].x, baseY), fcol);
        }
    }
    if(pts.size() >= 2) { dl->AddPolyline(pts.data(), (int)pts.size(), col, ImDrawFlags_None, style.thickness); }
    if(style.marker != Marker::None) {
        for(const ImVec2 &p : pts) { drawMarker(dl, style.marker, p, style.markerSize, col); }
    }
}

PointHit TimePlot::drawScatter(const char * /*id*/, PointGetter get, const void *data, int count, const ScatterStyle &style) {
    PointHit hit;
    if(!m_frameVisible || !get || count <= 0) { return hit; }
    ImDrawList *dl  = ImGui::GetWindowDrawList();
    const ImU32 col = style.color ? style.color : kPalette[(m_paletteCursor++) % kPaletteCount];
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const bool   cat   = (m_y.scale == AxisScale::Categorical);
    const int    n     = (int)m_lanes.size();
    float        best  = style.hoverRadiusPx * style.hoverRadiusPx;

    for(int i = 0; i < count; i++) {
        const PlotPoint p = get(data, i);
        accumulateX(p.x);
        const float sx = dataToScreenX(p.x);
        float sy;
        if(cat) {
            const int li = (int)std::lround(p.y);
            if(li < 0 || li >= n) { continue; }
            float y0, y1; laneBand(m_origin.y, m_size.y, li, n, y0, y1);
            sy = (y0 + y1) * 0.5f;
        } else {
            accumulateY(p.y);
            sy = dataToScreenY(p.y);
        }
        drawMarker(dl, style.marker, ImVec2(sx, sy), style.markerSize, col);
        if(m_areaHovered) {
            const float dx = mouse.x - sx, dy = mouse.y - sy, d2 = dx * dx + dy * dy;
            if(d2 < best) { best = d2; hit.index = i; }
        }
    }
    return hit;
}

SpanHit TimePlot::drawSpans(const char *laneId, SpanGetter get, const void *data, int count, const SpanStyle &style) {
    SpanHit hit;
    if(!m_frameVisible || !get || count <= 0) { return hit; }
    const int li = laneIndex(laneId);
    if(li < 0 || m_y.scale != AxisScale::Categorical) { return hit; }   // spans need a registered lane on a categorical Y

    ImDrawList *dl = ImGui::GetWindowDrawList();
    float y0, y1; laneBand(m_origin.y, m_size.y, li, (int)m_lanes.size(), y0, y1);
    y0 += 2.0f; y1 -= 2.0f;
    if(y1 <= y0) { return hit; }
    const ImU32  laneCol = m_lanes[(std::size_t)li].color ? m_lanes[(std::size_t)li].color : kPalette[li % kPaletteCount];
    const ImVec2 mouse   = ImGui::GetIO().MousePos;
    const bool   inLane  = m_areaHovered && mouse.y >= y0 && mouse.y <= y1;
    const float  clipLo  = m_origin.x, clipHi = m_origin.x + m_size.x;

    // Tracy LOD pass (pure; unit-tested via detail::collapseSpans). Reuse a scratch
    // vector so the per-frame allocation is amortized away.
    static thread_local std::vector<detail::SpanCluster> clusters;
    detail::collapseSpans(get, data, count, style.minSpanPx,
                          [this](double t) { return dataToScreenX(t); }, clusters);

    for(const detail::SpanCluster &cl : clusters) {
        accumulateX(cl.start); accumulateX(cl.end);
        float xa = dataToScreenX(cl.start), xb = dataToScreenX(cl.end);
        if(xb < xa) { std::swap(xa, xb); }

        if(!cl.collapsed) {
            const Span  s   = get(data, cl.first);                 // re-read for color/label
            const ImU32 c   = s.color ? s.color : laneCol;
            const float cxa = std::max(xa, clipLo), cxb = std::min(xb, clipHi);
            if(cxb > cxa) {
                dl->AddRectFilled(ImVec2(cxa, y0), ImVec2(cxb, y1), c, style.roundingPx);
                dl->AddRect(ImVec2(cxa, y0), ImVec2(cxb, y1), withAlpha(IM_COL32_WHITE, 0.15f), style.roundingPx);
                if(style.showLabels && s.label && (cxb - cxa) > 22.0f) {
                    const ImVec4 clip(cxa + 3.0f, y0, cxb - 3.0f, y1);
                    dl->AddText(nullptr, 0.0f, ImVec2(cxa + 4.0f, (y0 + y1) * 0.5f - ImGui::GetFontSize() * 0.5f),
                                withAlpha(IM_COL32_WHITE, 0.9f), s.label, nullptr, 0.0f, &clip);
                }
            }
        } else {
            // density tick: alpha grows with the merged count
            const float a    = ImClamp(0.25f + 0.05f * (float)cl.count, 0.25f, 0.85f);
            const float dcxa = std::max(xa, clipLo);
            const float dcxb = std::min(std::max(xb, xa + 1.0f), clipHi);
            if(dcxb > dcxa) { dl->AddRectFilled(ImVec2(dcxa, y0), ImVec2(dcxb, y1), withAlpha(laneCol, a)); }
        }
        if(inLane && mouse.x >= xa && mouse.x <= xb) {
            hit.index = cl.first; hit.mergedCount = cl.count; hit.range = Range<double>(cl.start, cl.end);
        }
    }
    return hit;
}

// ───────────────────────── TimePlot: endFrame (deferred fit/follow) ─────────────

void TimePlot::endFrame() {
    if(m_inFrame && m_frameVisible) {
        ImGui::GetWindowDrawList()->PopClipRect();   // matches the PushClipRect in begin()

        // deferred fit / follow for the NEXT frame (extent now known from this frame)
        if(m_xlink->follow && m_haveDataX) {
            double w = (m_xlink->followWindow > 0.0) ? m_xlink->followWindow : m_xlink->range.span();
            if(w <= 0.0) { w = 1.0; }
            m_xlink->range = Range<double>(m_dataXhi - w, m_dataXhi);
        } else if(m_x.autoFit && m_haveDataX) {
            double pad = (m_dataXhi - m_dataXlo) * 0.02;
            if(pad <= 0.0) { pad = 0.5; }
            m_xlink->range = Range<double>(m_dataXlo - pad, m_dataXhi + pad);
        }
        if(m_y.autoFit && m_haveDataY && m_y.scale != AxisScale::Categorical) {
            double pad = (m_dataYhi - m_dataYlo) * 0.05;
            if(pad <= 0.0) { pad = 1.0; }
            m_y.range = Range<double>(m_dataYlo - pad, m_dataYhi + pad);
        }
    }
    if(m_inFrame) { ImGui::EndChild(); ImGui::PopID(); }
    m_inFrame = false;
    m_frameVisible = false;
}

}  // namespace imtool::plot

#include <imtool/settings/setting.hpp>

#include <fstream>

#include <imgui.h>

// The Settings framework's only ImGui-touching translation unit: the per-type
// drawWidget renderers, the SettingBase/SettingGroup draw chrome, and the file-IO
// boundary. ImGui core widgets only (no GLFW/GL), so this links into the
// backend-light core lib and the headless tests link it without a display.

namespace imtool {

// --- per-type widgets --------------------------------------------------------
// Each renders the widget only (label column + PushID handled by SettingBase::draw).
// The "##v" label is unique within the per-setting PushID scope.

bool drawWidget(bool &v, const SettingMeta<bool> & /*m*/) {
    return ImGui::Checkbox("##v", &v);
}

bool drawWidget(int &v, const SettingMeta<int> &m) {
    const char *fmt = m.format.empty() ? "%d" : m.format.c_str();
    if(m.min && m.max) { return ImGui::SliderInt("##v", &v, *m.min, *m.max, fmt); }
    return ImGui::DragInt("##v", &v, m.step != 0 ? static_cast<float>(m.step) : 1.0f,
                          m.min ? *m.min : 0, m.max ? *m.max : 0, fmt);
}

bool drawWidget(float &v, const SettingMeta<float> &m) {
    const char *fmt = m.format.empty() ? "%.3f" : m.format.c_str();
    if(m.hint != WidgetHint::Drag && m.min && m.max) { return ImGui::SliderFloat("##v", &v, *m.min, *m.max, fmt); }
    return ImGui::DragFloat("##v", &v, m.step != 0.0f ? m.step : 0.1f,
                            m.min ? *m.min : 0.0f, m.max ? *m.max : 0.0f, fmt);
}

bool drawWidget(Vec2f &v, const SettingMeta<Vec2f> &m) {
    float t[2] = {v.x, v.y};
    const char *fmt   = m.format.empty() ? "%.3f" : m.format.c_str();
    const bool  ch    = ImGui::DragFloat2("##v", t, m.step.x != 0.0f ? m.step.x : 0.1f,
                                          m.min ? m.min->x : 0.0f, m.max ? m.max->x : 0.0f, fmt);
    if(ch) { v.x = t[0]; v.y = t[1]; }
    return ch;
}

bool drawWidget(Vec3f &v, const SettingMeta<Vec3f> &m) {
    float t[3] = {v.x, v.y, v.z};
    const char *fmt = m.format.empty() ? "%.3f" : m.format.c_str();
    const bool  ch  = ImGui::DragFloat3("##v", t, m.step.x != 0.0f ? m.step.x : 0.1f,
                                        m.min ? m.min->x : 0.0f, m.max ? m.max->x : 0.0f, fmt);
    if(ch) { v.x = t[0]; v.y = t[1]; v.z = t[2]; }
    return ch;
}

bool drawWidget(Vec4f &v, const SettingMeta<Vec4f> &m) {
    float t[4] = {v.x, v.y, v.z, v.w};
    bool  ch;
    if(m.hint == WidgetHint::Color) {
        ch = ImGui::ColorEdit4("##v", t);
    } else {
        const char *fmt = m.format.empty() ? "%.3f" : m.format.c_str();
        ch = ImGui::DragFloat4("##v", t, m.step.x != 0.0f ? m.step.x : 0.1f,
                               m.min ? m.min->x : 0.0f, m.max ? m.max->x : 0.0f, fmt);
    }
    if(ch) { v.x = t[0]; v.y = t[1]; v.z = t[2]; v.w = t[3]; }
    return ch;
}

namespace {
// std::string-backed InputText with dynamic resize — self-contained, so the
// toolkit doesn't require the consumer's ImGui build to include misc/cpp/imgui_stdlib.
struct StrResizeData { std::string *str; };
int strResizeCallback(ImGuiInputTextCallbackData *data) {
    if(data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        auto *ud = static_cast<StrResizeData *>(data->UserData);
        ud->str->resize(static_cast<std::size_t>(data->BufTextLen));
        data->Buf = ud->str->data();
    }
    return 0;
}
}  // namespace

bool drawWidget(std::string &v, const SettingMeta<std::string> & /*m*/) {
    StrResizeData ud{&v};
    return ImGui::InputText("##v", v.data(), v.capacity() + 1,
                            ImGuiInputTextFlags_CallbackResize, strResizeCallback, &ud);
}

// --- chrome ------------------------------------------------------------------

bool SettingBase::draw(float scale) {
    ImGui::PushID(id_.c_str());
    if(!label_.empty()) {
        ImGui::TextUnformatted(label_.c_str());
        if(!tooltip_.empty() && ImGui::IsItemHovered()) { ImGui::SetTooltip("%s", tooltip_.c_str()); }
        ImGui::SameLine(150.0f * scale);
        ImGui::SetNextItemWidth(-1.0f);
    }
    const bool changed = onDrawWidget(scale);
    ImGui::PopID();
    return changed;
}

bool SettingGroup::draw(float scale) {
    bool changed = false;
    ImGui::PushID(id_.c_str());
    // CollapsingHeader is End-less, so a plain if() is correct (no scope guard needed).
    const bool open = label_.empty() ? true
                                     : ImGui::CollapsingHeader(label_.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
    if(open) {
        if(!label_.empty()) { ImGui::Indent(); }
        for(auto &c : children_) { changed = c->draw(scale) || changed; }
        if(!label_.empty()) { ImGui::Unindent(); }
    }
    ImGui::PopID();
    return changed;
}

// --- file IO (std::expected boundary) ----------------------------------------

std::expected<void, ToolkitError> saveSettingsFile(const SettingGroup &group, const std::filesystem::path &path) {
    std::ofstream os(path);
    if(!os) { return std::unexpected(ToolkitError{ErrorKind::IoFailure, "cannot open file for writing", path.string()}); }
    os << group.to_json().dump(2) << '\n';
    if(!os) { return std::unexpected(ToolkitError{ErrorKind::IoFailure, "write failed", path.string()}); }
    return {};
}

std::expected<void, ToolkitError> loadSettingsFile(SettingGroup &group, const std::filesystem::path &path) {
    std::ifstream is(path);
    if(!is) { return std::unexpected(ToolkitError{ErrorKind::IoFailure, "cannot open file for reading", path.string()}); }
    nlohmann::json js;
    try {
        is >> js;
    } catch(const nlohmann::json::exception &e) {
        return std::unexpected(ToolkitError{ErrorKind::JsonParse, e.what(), path.string()});
    }
    group.from_json(js);   // per-entry load is log+skip inside; not an error
    return {};
}

}  // namespace imtool

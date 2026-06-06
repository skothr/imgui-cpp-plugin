#include <imtool/input/key_binding.hpp>

#include <cfloat>
#include <string>

#include <imtool/common/logging.hpp>

namespace imtool {

std::string KeyChord::toString() const {
    if(!valid()) { return {}; }
    std::string s;
    if(chord & ImGuiMod_Ctrl)  { s += "Ctrl+"; }
    if(chord & ImGuiMod_Shift) { s += "Shift+"; }
    if(chord & ImGuiMod_Alt)   { s += "Alt+"; }
    if(chord & ImGuiMod_Super) { s += "Super+"; }
    s += ImGui::GetKeyName(key());   // needs a live ImGui context (editor-only call site)
    return s;
}

// --- registration -----------------------------------------------------------

void KeyBindingManager::registerAction(const KeyBindingDesc &desc) {
    KeyBinding b;
    b.action       = std::string(desc.action);
    b.label        = std::string(desc.label);
    b.description  = std::string(desc.description);
    b.chord        = KeyChord(desc.defaultChord);
    b.defaultChord = KeyChord(desc.defaultChord);
    b.repeat       = desc.repeat;
    b.callback     = desc.callback;

    const auto it = m_index.find(b.action);
    if(it != m_index.end()) { m_bindings[it->second] = std::move(b); }   // re-register overwrites
    else {
        m_index[b.action] = m_bindings.size();
        m_bindings.push_back(std::move(b));
    }
}

void KeyBindingManager::registerAll(const std::vector<KeyBindingDesc> &defaults) {
    for(const auto &d : defaults) { registerAction(d); }
}

void KeyBindingManager::clear() {
    m_bindings.clear();
    m_index.clear();
    m_recording.reset();
}

// --- lookup ------------------------------------------------------------------

const KeyBinding* KeyBindingManager::findBinding(std::string_view action) const {
    const auto it = m_index.find(std::string(action));
    return (it != m_index.end()) ? &m_bindings[it->second] : nullptr;
}
KeyBinding* KeyBindingManager::findBindingMutable(std::string_view action) {
    const auto it = m_index.find(std::string(action));
    return (it != m_index.end()) ? &m_bindings[it->second] : nullptr;
}
const KeyBinding* KeyBindingManager::find(std::string_view action) const { return findBinding(action); }

const KeyBinding* KeyBindingManager::bindingFor(KeyChord chord, std::string_view excludeAction) const {
    if(!chord.valid()) { return nullptr; }
    for(const auto &b : m_bindings) {
        if(b.action == excludeAction) { continue; }
        if(b.chord == chord)          { return &b; }
    }
    return nullptr;
}

// --- rebinding ---------------------------------------------------------------

bool KeyBindingManager::rebind(std::string_view action, KeyChord chord) {
    KeyBinding *b = findBindingMutable(action);
    if(!b) { return false; }
    b->chord = chord;
    return true;
}
bool KeyBindingManager::resetToDefault(std::string_view action) {
    KeyBinding *b = findBindingMutable(action);
    if(!b) { return false; }
    b->chord = b->defaultChord;
    return true;
}
void KeyBindingManager::resetAllToDefault() {
    for(auto &b : m_bindings) { b.chord = b.defaultChord; }
}

// --- query -------------------------------------------------------------------

bool KeyBindingManager::captureBlocked() const {
    return m_respectCapture && ImGui::GetIO().WantCaptureKeyboard;
}

bool KeyBindingManager::chordActive(const KeyChord &c, bool repeat) const {
    if(!c.valid()) { return false; }
    if((ImGui::GetIO().KeyMods & ImGuiMod_Mask_) != c.mods()) { return false; }   // exact modifiers
    return ImGui::IsKeyPressed(c.key(), repeat);
}

bool KeyBindingManager::triggered(std::string_view action) const {
    const KeyBinding *b = findBinding(action);
    if(!b || captureBlocked()) { return false; }
    return chordActive(b->chord, b->repeat);
}

bool KeyBindingManager::held(std::string_view action) const {
    const KeyBinding *b = findBinding(action);
    if(!b || !b->chord.valid() || captureBlocked()) { return false; }
    if((ImGui::GetIO().KeyMods & ImGuiMod_Mask_) != b->chord.mods()) { return false; }
    return ImGui::IsKeyDown(b->chord.key());
}

int KeyBindingManager::poll() {
    if(m_recording) { return 0; }   // don't fire actions while capturing a rebind
    int fired = 0;
    for(const auto &b : m_bindings) {
        if(b.callback && triggered(b.action)) { b.callback(); ++fired; }
    }
    return fired;
}

// --- recording ---------------------------------------------------------------

namespace {
[[nodiscard]] bool isModifierKey(ImGuiKey k) {
    return k == ImGuiKey_LeftCtrl  || k == ImGuiKey_RightCtrl  ||
           k == ImGuiKey_LeftShift || k == ImGuiKey_RightShift ||
           k == ImGuiKey_LeftAlt   || k == ImGuiKey_RightAlt   ||
           k == ImGuiKey_LeftSuper || k == ImGuiKey_RightSuper;
}
}  // namespace

void KeyBindingManager::beginRecording(std::string_view action) {
    if(findBinding(action)) { m_recording = std::string(action); }
}
void KeyBindingManager::cancelRecording() { m_recording.reset(); }

bool KeyBindingManager::updateRecording() {
    if(!m_recording) { return false; }
    if(ImGui::IsKeyPressed(ImGuiKey_Escape, false)) { m_recording.reset(); return true; }
    for(int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k) {
        const ImGuiKey key = static_cast<ImGuiKey>(k);
        if(isModifierKey(key)) { continue; }
        if(ImGui::IsKeyPressed(key, false)) {
            rebind(*m_recording, KeyChord(key, ImGui::GetIO().KeyMods & ImGuiMod_Mask_));
            m_recording.reset();
            return true;
        }
    }
    return false;
}

// --- serialization -----------------------------------------------------------

nlohmann::json KeyBindingManager::toJson() const {
    nlohmann::json js = nlohmann::json::object();
    for(const auto &b : m_bindings) { js[b.action] = b.chord; }   // to_json(KeyChord)
    return js;
}

bool KeyBindingManager::loadJson(const nlohmann::json &js) {
    if(!js.is_object()) {
        log() << LogLevel::Error << "KeyBindingManager::loadJson: root is not an object"; log().flush();
        return false;
    }
    for(const auto &[action, val] : js.items()) {
        KeyBinding *b = findBindingMutable(action);
        if(!b) { continue; }   // unknown action: ignore (forward/backward compatible)
        try { b->chord = val.get<KeyChord>(); }
        catch(const std::exception &e) {
            log() << LogLevel::Warning << "KeyBindingManager::loadJson: bad entry for '" << action << "': " << e.what();
            log().flush();
        }
    }
    return true;
}

// --- editor ------------------------------------------------------------------

bool KeyBindingManager::drawEditor(const char *title) {
    bool changed = false;
    if(!ImGui::Begin(title)) { ImGui::End(); return false; }   // top-level Begin always paired with End

    if(isRecording()) { if(updateRecording()) { changed = true; } }

    if(ImGui::BeginTable("##imtool-keybindings", 3,
                         ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Action");
        ImGui::TableSetupColumn("Shortcut");
        ImGui::TableSetupColumn("");
        ImGui::TableHeadersRow();
        for(const auto &b : m_bindings) {
            ImGui::TableNextRow();
            ImGui::PushID(b.action.c_str());

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(b.label.empty() ? b.action.c_str() : b.label.c_str());
            if(!b.description.empty() && ImGui::IsItemHovered()) { ImGui::SetTooltip("%s", b.description.c_str()); }

            ImGui::TableNextColumn();
            const bool recordingThis = isRecording() && (*m_recording == b.action);
            const std::string shortcut = recordingThis ? "press a key..."
                                       : (b.chord.valid() ? b.chord.toString() : "(unbound)");
            if(ImGui::Button(shortcut.c_str(), ImVec2(-FLT_MIN, 0.0f))) { beginRecording(b.action); }

            ImGui::TableNextColumn();
            ImGui::BeginDisabled(b.isDefault());
            if(ImGui::SmallButton("reset")) { resetToDefault(b.action); changed = true; }
            ImGui::EndDisabled();

            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::End();
    return changed;
}

}  // namespace imtool

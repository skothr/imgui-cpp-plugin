#pragma once

// KeyBindingManager — configurable action -> key-chord mapping, queried against
// ImGui IO each frame. Lifted + modernized from 19-logos keyBinding.hpp and
// 16-nodegraph keyManager/keyBinding: the GLFW key-name table, the std::thread
// update loop, the mutex, and the multi-stroke sequence queue are all dropped —
// modern ImGui packs key+mods into one ImGuiKeyChord, and queries poll IO
// directly, so there is no cross-thread state and no manual event plumbing.
//
// Deferred (post-beta): human-readable chord (de)serialization with an ImGuiKey
// name table (beta serializes the raw chord int); multi-chord sequences;
// per-binding GLOBAL/EXTRA_MODS flags; mod multipliers.

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <imgui.h>
#include <nlohmann/json.hpp>

namespace imtool {

// A key + modifier-mask shortcut: an ImGuiKey OR-ed with ImGuiMod_* values.
struct KeyChord {
    ImGuiKeyChord chord = ImGuiKey_None;   // 0 == unbound

    KeyChord() = default;
    KeyChord(ImGuiKeyChord c) : chord(c) {}                       // implicit: bind with `ImGuiMod_Ctrl | ImGuiKey_S`
    KeyChord(ImGuiKey key, ImGuiKeyChord mods) : chord(key | mods) {}

    [[nodiscard]] ImGuiKey      key()   const { return static_cast<ImGuiKey>(chord & ~ImGuiMod_Mask_); }
    [[nodiscard]] ImGuiKeyChord mods()  const { return chord & ImGuiMod_Mask_; }
    [[nodiscard]] bool          valid() const { return (chord & ~ImGuiMod_Mask_) != ImGuiKey_None; }

    [[nodiscard]] bool operator==(const KeyChord &o) const { return chord == o.chord; }
    [[nodiscard]] bool operator!=(const KeyChord &o) const { return chord != o.chord; }

    // Human label e.g. "Ctrl+S" via ImGui::GetKeyName — requires a live ImGui
    // context, so call only inside a frame (the editor uses it). Empty if unbound.
    [[nodiscard]] std::string toString() const;
};

// JSON: the raw chord int (context-free, testable). A human-string form that
// survives ImGui enum churn is a post-beta improvement.
inline void to_json(nlohmann::json &js, const KeyChord &c) { js = c.chord; }
inline void from_json(const nlohmann::json &js, KeyChord &c) { c.chord = js.get<ImGuiKeyChord>(); }

// One named action with its current + default chord.
struct KeyBinding {
    std::string           action;          // stable id / map key, e.g. "edit.undo"
    std::string           label;           // human label ("Undo")
    std::string           description;     // tooltip
    KeyChord              chord;            // current (mutable at runtime)
    KeyChord              defaultChord;     // for reset + dirty check
    bool                  repeat = false;  // IsKeyPressed repeat flag for triggered()
    std::function<void()> callback = nullptr;  // optional auto-fire in poll()

    [[nodiscard]] bool isDefault() const { return chord == defaultChord; }
};

// Registration record (the "declare these defaults" shape).
struct KeyBindingDesc {
    std::string_view      action;
    std::string_view      label;
    std::string_view      description  = {};
    ImGuiKeyChord         defaultChord = ImGuiKey_None;
    bool                  repeat       = false;
    std::function<void()> callback     = nullptr;
};

class KeyBindingManager {
public:
    KeyBindingManager() = default;
    explicit KeyBindingManager(const std::vector<KeyBindingDesc> &defaults) { registerAll(defaults); }

    // ---- registration ----
    void registerAction(const KeyBindingDesc &desc);
    void registerAll(const std::vector<KeyBindingDesc> &defaults);
    void clear();

    // ---- query (call once per frame, after ImGui::NewFrame; need a live context) ----
    [[nodiscard]] bool triggered(std::string_view action) const;   // pressed this frame (edge)
    [[nodiscard]] bool held(std::string_view action) const;        // currently down
    int  poll();   // fire callbacks for every triggered binding; returns count fired

    // ---- rebinding (context-free) ----
    [[nodiscard]] const KeyBinding* find(std::string_view action) const;
    bool rebind(std::string_view action, KeyChord chord);          // false on unknown action
    bool resetToDefault(std::string_view action);                  // false on unknown action
    void resetAllToDefault();
    // The action currently bound to `chord` (excluding `excludeAction`), or nullptr.
    [[nodiscard]] const KeyBinding* bindingFor(KeyChord chord, std::string_view excludeAction = {}) const;

    // ---- recording (interactive capture; needs a live context) ----
    void beginRecording(std::string_view action);
    void cancelRecording();
    [[nodiscard]] bool isRecording() const { return m_recording.has_value(); }
    bool updateRecording();   // call each frame while recording; commits on first key, true when done

    // ---- capture gating ----
    void setRespectImGuiCapture(bool v) { m_respectCapture = v; }
    [[nodiscard]] bool respectsImGuiCapture() const { return m_respectCapture; }

    // ---- access ----
    [[nodiscard]] const std::vector<KeyBinding>& bindings() const { return m_bindings; }
    [[nodiscard]] std::size_t size() const { return m_bindings.size(); }

    // ---- serialization (context-free) ----
    [[nodiscard]] nlohmann::json toJson() const;   // { "<action>": <chord int>, ... }
    bool loadJson(const nlohmann::json &js);        // false only on non-object root; bad entries skipped+logged

    // ---- editor (needs a live context) ----
    bool drawEditor(const char *title = "Key Bindings");   // returns true if any binding changed

private:
    [[nodiscard]] const KeyBinding* findBinding(std::string_view action) const;
    [[nodiscard]] KeyBinding*       findBindingMutable(std::string_view action);
    [[nodiscard]] bool              chordActive(const KeyChord &c, bool repeat) const;  // edge query helper
    [[nodiscard]] bool              captureBlocked() const;

    std::vector<KeyBinding>                       m_bindings;        // stable order for the editor
    std::unordered_map<std::string, std::size_t>  m_index;           // action -> idx
    std::optional<std::string>                    m_recording;       // action being rebound
    bool                                          m_respectCapture = true;
};

}  // namespace imtool

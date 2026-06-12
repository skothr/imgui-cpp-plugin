#include "imtest.hpp"

#include <imtool/input/key_binding.hpp>

using namespace imtool;

// Exercises only the context-free surface (registration, rebind, collision,
// reset, JSON, KeyChord decode). triggered()/held()/poll()/drawEditor() need a
// live ImGui frame and are covered by running the scaffold.
int main() {
    IMTEST_SUITE("keybindings");

    // KeyChord decode
    KeyChord undo(ImGuiMod_Ctrl | ImGuiKey_Z);
    CHECK(undo.valid());
    CHECK(undo.key() == ImGuiKey_Z);
    CHECK(undo.mods() == ImGuiMod_Ctrl);
    CHECK(!KeyChord().valid());
    CHECK(KeyChord(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S).mods() == (ImGuiMod_Ctrl | ImGuiMod_Shift));

    KeyBindingManager km;
    km.registerAll({
        {"edit.undo", "Undo", "Undo last action", ImGuiMod_Ctrl | ImGuiKey_Z, false, nullptr},
        {"edit.redo", "Redo", "Redo",             ImGuiMod_Ctrl | ImGuiKey_Y, false, nullptr},
        {"file.save", "Save", "Save session",     ImGuiMod_Ctrl | ImGuiKey_S, false, nullptr},
    });
    CHECK(km.size() == 3);
    CHECK(km.find("edit.undo") != nullptr);
    CHECK(km.find("nope") == nullptr);
    CHECK(km.find("edit.undo")->chord.key() == ImGuiKey_Z);

    // PR #2 review (skothr-cc, key_binding.hpp): a pointer returned by find() must
    // stay valid across a LATER registerAction() — the late-registration-dangles-an-
    // earlier-pointer case (a plugin registering its own actions after a UI cached a
    // chord pointer). m_bindings is a std::deque precisely so element addresses are
    // stable across push_back; register many more actions, then re-check the pointer.
    {
        const KeyBinding *undo_ptr = km.find("edit.undo");
        for(int i = 0; i < 64; i++) {
            km.registerAction({"late.action", "Late", "", ImGuiKey_None, false, nullptr});  // distinct ids below
            km.registerAction({("late.action." + std::to_string(i)), "Late", "", ImGuiKey_None, false, nullptr});
        }
        CHECK(km.find("edit.undo") == undo_ptr);           // same address — not relocated
        CHECK(undo_ptr->chord.key() == ImGuiKey_Z);        // and still reads correctly (no dangle)
    }

    // rebind
    CHECK(km.rebind("edit.undo", KeyChord(ImGuiMod_Ctrl | ImGuiKey_W)));
    CHECK(km.find("edit.undo")->chord.key() == ImGuiKey_W);
    CHECK(!km.rebind("nope", KeyChord(ImGuiKey_A)));

    // collision detection
    const KeyBinding *save = km.bindingFor(KeyChord(ImGuiMod_Ctrl | ImGuiKey_S));
    CHECK(save != nullptr);
    CHECK(save->action == "file.save");
    CHECK(km.bindingFor(KeyChord(ImGuiMod_Ctrl | ImGuiKey_S), "file.save") == nullptr);  // excluded
    CHECK(km.bindingFor(KeyChord(ImGuiKey_Q)) == nullptr);                                // unbound

    // reset
    CHECK(!km.find("edit.undo")->isDefault());
    CHECK(km.resetToDefault("edit.undo"));
    CHECK(km.find("edit.undo")->isDefault());
    CHECK(km.find("edit.undo")->chord.key() == ImGuiKey_Z);

    // JSON round-trip
    km.rebind("file.save", KeyChord(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S));
    const nlohmann::json js = km.toJson();
    KeyBindingManager km2;
    km2.registerAll({
        {"edit.undo", "Undo", "", ImGuiMod_Ctrl | ImGuiKey_Z, false, nullptr},
        {"file.save", "Save", "", ImGuiMod_Ctrl | ImGuiKey_S, false, nullptr},
    });
    CHECK(km2.loadJson(js));
    CHECK(km2.find("file.save")->chord == KeyChord(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S));
    CHECK(km2.find("edit.undo")->chord.key() == ImGuiKey_Z);   // unchanged by load
    CHECK(!km2.loadJson(nlohmann::json::array()));             // non-object root rejected

    // MAIN-380: the recording-range predicate is now a pure header function, so the
    // policy (which keys may be captured as a binding) is testable without a frame.
    CHECK(isRecordableKey(ImGuiKey_A));            // real named key
    CHECK(isRecordableKey(ImGuiKey_Escape));       // named key (updateRecording treats it as cancel separately)
    CHECK(isRecordableKey(ImGuiKey_F5));
    CHECK(!isRecordableKey(ImGuiKey_LeftCtrl));    // modifier alias -> captured as the mod mask, not the key
    CHECK(!isRecordableKey(ImGuiKey_RightSuper));
    CHECK(!isRecordableKey(ImGuiKey_None));        // below NamedKey_BEGIN
    CHECK(!isRecordableKey(ImGuiKey_GamepadStart)); // gamepad block
    CHECK(!isRecordableKey(ImGuiKey_MouseLeft));    // mouse button (binding it fires on click)
    CHECK(isModifierKey(ImGuiKey_LeftShift));
    CHECK(!isModifierKey(ImGuiKey_A));

    return imtest::report();
}

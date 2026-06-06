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

    return imtest::report();
}

#include "imtest.hpp"

#include <memory>

#include <imtool/command/command.hpp>

// Headless coverage of the undo/redo stack (Epic C): execute-on-push, undo/redo
// walking, redo-branch clearing, depth bounding, labels, and coalescing.

using namespace imtool;

namespace {
// A coalescing "set int" command: a contiguous run of sets to the same target
// folds into one undo entry (the slider-drag pattern).
struct SetIntCommand : Command {
    int *target;
    int  old_v;
    int  new_v;
    SetIntCommand(int *t, int v) : target(t), old_v(*t), new_v(v) {}
    void apply() override { *target = new_v; }
    void revert() override { *target = old_v; }
    [[nodiscard]] std::string_view label() const override { return "Set"; }
    [[nodiscard]] bool mergeWith(const Command &next) override {
        if(const auto *o = dynamic_cast<const SetIntCommand *>(&next); o && o->target == target) {
            new_v = o->new_v;   // keep our original old_v, adopt the latest new_v
            return true;
        }
        return false;
    }
};
}  // namespace

int main() {
    IMTEST_SUITE("command");

    int       x = 0;
    UndoStack st;

    // push() executes immediately + records
    st.push("set 1", [&] { x = 1; }, [&] { x = 0; });
    CHECK(x == 1);
    st.push("set 2", [&] { x = 2; }, [&] { x = 1; });
    CHECK(x == 2);
    CHECK(st.canUndo() && !st.canRedo());
    CHECK(st.undoCount() == 2);
    CHECK(st.undoLabel() == "set 2");

    // undo walks back; redo becomes available
    CHECK(st.undo()); CHECK(x == 1);
    CHECK(st.canRedo() && st.redoLabel() == "set 2");
    CHECK(st.undo()); CHECK(x == 0);
    CHECK(!st.canUndo());
    CHECK(!st.undo());                    // nothing left to undo

    // redo re-applies
    CHECK(st.redo()); CHECK(x == 1);
    CHECK(st.redo()); CHECK(x == 2);
    CHECK(!st.redo());

    // a new push after undo clears the redo branch
    CHECK(st.undo()); CHECK(x == 1);
    CHECK(st.canRedo());
    st.push("set 9", [&] { x = 9; }, [&] { x = 1; });
    CHECK(x == 9);
    CHECK(!st.canRedo());

    st.clear();
    CHECK(!st.canUndo() && !st.canRedo() && st.undoCount() == 0);

    // max-depth evicts the oldest entries
    {
        int       y = 0;
        UndoStack bounded(3);
        for(int i = 1; i <= 5; i++) { bounded.push("inc", [&, i] { y = i; }, [&, i] { y = i - 1; }); }
        CHECK(bounded.undoCount() == 3);   // only the last 3 retained
        CHECK(y == 5);
    }

    // coalescing: a run of SetIntCommands to the same target -> one undo entry that
    // reverts to the value before the whole run
    {
        int       z = 0;
        UndoStack cs;
        cs.push(std::make_unique<SetIntCommand>(&z, 10));
        cs.push(std::make_unique<SetIntCommand>(&z, 20));
        cs.push(std::make_unique<SetIntCommand>(&z, 30));
        CHECK(z == 30);
        CHECK(cs.undoCount() == 1);
        CHECK(cs.undo()); CHECK(z == 0);    // back to before the run, not just the last set
        CHECK(cs.redo()); CHECK(z == 30);
    }

    return imtest::report();
}

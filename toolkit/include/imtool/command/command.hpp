#pragma once

// Undo/redo (Epic C) — an abstract command-pattern stack, reusable across the
// whole toolkit (NodeGraph, settings inspectors, any UI that mutates app state),
// deliberately NOT NodeGraph-specific. Header-only, std-only (no ImGui, no Node),
// so any layer can depend on it.
//
// A Command knows how to apply() and revert() one reversible operation. UndoStack
// executes-and-records on push(), and walks the history on undo()/redo(). The
// ergonomic path is push(label, applyFn, revertFn) — a lambda pair. A typed
// Command subclass can additionally coalesce a continuous edit (e.g. a slider
// drag) into one undo entry via mergeWith().

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace imtool {

// One reversible operation. apply() performs it (and re-performs on redo);
// revert() undoes it. The pair must be exact inverses for a clean history.
class Command {
public:
    virtual ~Command() = default;
    virtual void                   apply()  = 0;
    virtual void                   revert() = 0;
    [[nodiscard]] virtual std::string_view label() const = 0;

    // Optional coalescing. When UndoStack::push receives `next` and the current top
    // command returns true here (after absorbing next's effect into itself), `next`
    // is folded into the top instead of pushed as a separate entry — so a drag that
    // fires many small edits collapses to one undo step. Default: no coalescing.
    [[nodiscard]] virtual bool mergeWith(const Command & /*next*/) { return false; }
};

// A Command built from a label + two lambdas — the common case.
class FunctionalCommand : public Command {
public:
    FunctionalCommand(std::string label, std::function<void()> apply, std::function<void()> revert)
        : label_(std::move(label)), apply_(std::move(apply)), revert_(std::move(revert)) {}
    void apply()  override { if(apply_)  { apply_(); } }
    void revert() override { if(revert_) { revert_(); } }
    [[nodiscard]] std::string_view label() const override { return label_; }

private:
    std::string           label_;
    std::function<void()> apply_;
    std::function<void()> revert_;
};

// Undo/redo history. push() EXECUTES the command then records it (clearing the redo
// branch); undo()/redo() walk it. Bounded by max_depth (oldest entry dropped).
class UndoStack {
public:
    explicit UndoStack(std::size_t max_depth = 256) : max_depth_(max_depth) {}

    // Execute `cmd` (apply) and record it. Clears the redo branch. If the current
    // top command coalesces `cmd` (mergeWith), `cmd` is folded in rather than pushed.
    void push(std::unique_ptr<Command> cmd) {
        if(!cmd) { return; }
        cmd->apply();
        redo_.clear();
        if(!undo_.empty() && undo_.back()->mergeWith(*cmd)) { return; }   // merged into the top entry
        undo_.push_back(std::move(cmd));
        while(undo_.size() > max_depth_) { undo_.erase(undo_.begin()); }
    }
    // Convenience: build a FunctionalCommand from a label + lambdas, then push it.
    void push(std::string label, std::function<void()> apply, std::function<void()> revert) {
        push(std::make_unique<FunctionalCommand>(std::move(label), std::move(apply), std::move(revert)));
    }

    bool undo() {
        if(undo_.empty()) { return false; }
        undo_.back()->revert();
        redo_.push_back(std::move(undo_.back()));
        undo_.pop_back();
        return true;
    }
    bool redo() {
        if(redo_.empty()) { return false; }
        redo_.back()->apply();
        undo_.push_back(std::move(redo_.back()));
        redo_.pop_back();
        return true;
    }

    [[nodiscard]] bool             canUndo()   const { return !undo_.empty(); }
    [[nodiscard]] bool             canRedo()   const { return !redo_.empty(); }
    [[nodiscard]] std::string_view undoLabel() const { return undo_.empty() ? std::string_view{} : undo_.back()->label(); }
    [[nodiscard]] std::string_view redoLabel() const { return redo_.empty() ? std::string_view{} : redo_.back()->label(); }
    [[nodiscard]] std::size_t      undoCount() const { return undo_.size(); }
    [[nodiscard]] std::size_t      redoCount() const { return redo_.size(); }
    void clear() { undo_.clear(); redo_.clear(); }

private:
    std::vector<std::unique_ptr<Command>> undo_;
    std::vector<std::unique_ptr<Command>> redo_;
    std::size_t                           max_depth_;
};

}  // namespace imtool

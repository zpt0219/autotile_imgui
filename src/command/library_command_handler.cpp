#include "library_command_handler.h"

namespace atm {

LibraryCommandHandler::LibraryCommandHandler(size_t max_history_size)
    : max_history_size_(max_history_size) {}

EditorResult LibraryCommandHandler::add_and_execute_command(
    std::unique_ptr<LibraryCommand> command,
    LibraryHandler& handler,
    LibraryCallbacks* cb
) {
    if (!command) return EditorResult::Error("Null command passed to handler");

    // A merged edit lands on the undo stack just like a fresh one, so both
    // paths have to invalidate redo — but only once the edit has actually
    // gone through, or a rejected command would throw away a valid redo.
    if (!undo_stack_.empty() && undo_stack_.back()->merge_with(command.get())) {
        EditorResult res = undo_stack_.back()->execute(handler, cb);
        if (res.success) redo_stack_.clear();
        return res;
    }

    EditorResult res = command->execute(handler, cb);
    if (!res.success) return res;

    redo_stack_.clear();
    undo_stack_.push_back(std::move(command));
    if (undo_stack_.size() > max_history_size_) {
        undo_stack_.pop_front();
    }

    return res;
}

EditorResult LibraryCommandHandler::undo(LibraryHandler& handler, LibraryCallbacks* cb) {
    if (undo_stack_.empty()) return EditorResult::Error("Nothing to undo");

    auto cmd = std::move(undo_stack_.back());
    undo_stack_.pop_back();

    EditorResult res = cmd->undo(handler, cb);
    if (!res.success) return res;

    redo_stack_.push_back(std::move(cmd));
    return res;
}

EditorResult LibraryCommandHandler::redo(LibraryHandler& handler, LibraryCallbacks* cb) {
    if (redo_stack_.empty()) return EditorResult::Error("Nothing to redo");

    auto cmd = std::move(redo_stack_.back());
    redo_stack_.pop_back();

    EditorResult res = cmd->execute(handler, cb);
    if (!res.success) return res;

    undo_stack_.push_back(std::move(cmd));
    return res;
}

void LibraryCommandHandler::clear_history() {
    undo_stack_.clear();
    redo_stack_.clear();
}

} // namespace atm

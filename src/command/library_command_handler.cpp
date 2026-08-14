#include "library_command_handler.h"
#include <chrono>

namespace atm {

LibraryCommandHandler::LibraryCommandHandler(size_t max_history_size)
    : max_history_size_(max_history_size) {}

EditorResult LibraryCommandHandler::add_and_execute_command(
    std::unique_ptr<LibraryCommand> command,
    LibraryHandler& handler,
    LibraryCallbacks* cb
) {
    if (!command) return EditorResult::Error("Null command passed to handler");

    int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
    command->set_timestamp(now_ms);

    // Try merge with the top of undo stack if applicable
    if (!undo_stack_.empty()) {
        auto& top = undo_stack_.back();
        if (top->get_kind() == command->get_kind() &&
            top->get_target_hash() == command->get_target_hash() &&
            top->merge_with(command.get())) {
            // Merged successfully into top command, now re-execute top command
            return top->execute(handler, cb);
        }
    }

    EditorResult res = command->execute(handler, cb);
    if (!res.success) return res;

    undo_stack_.push_back(std::move(command));
    if (undo_stack_.size() > max_history_size_) {
        undo_stack_.pop_front();
    }
    redo_stack_.clear();

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

    EditorResult res = cmd->redo(handler, cb);
    if (!res.success) return res;

    undo_stack_.push_back(std::move(cmd));
    return res;
}

void LibraryCommandHandler::clear_history() {
    undo_stack_.clear();
    redo_stack_.clear();
}

} // namespace atm

#pragma once

#include "library_command.h"
#include <deque>
#include <memory>

namespace atm {

class LibraryCommandHandler {
public:
    explicit LibraryCommandHandler(size_t max_history_size = 100);
    ~LibraryCommandHandler() = default;

    EditorResult add_and_execute_command(
        std::unique_ptr<LibraryCommand> command,
        LibraryHandler& handler,
        LibraryCallbacks* cb = nullptr
    );

    EditorResult undo(LibraryHandler& handler, LibraryCallbacks* cb = nullptr);
    EditorResult redo(LibraryHandler& handler, LibraryCallbacks* cb = nullptr);

    bool can_undo() const { return !undo_stack_.empty(); }
    bool can_redo() const { return !redo_stack_.empty(); }

    void clear_history();

    size_t undo_count() const { return undo_stack_.size(); }
    size_t redo_count() const { return redo_stack_.size(); }

private:
    size_t max_history_size_;
    std::deque<std::unique_ptr<LibraryCommand>> undo_stack_;
    std::deque<std::unique_ptr<LibraryCommand>> redo_stack_;
};

} // namespace atm

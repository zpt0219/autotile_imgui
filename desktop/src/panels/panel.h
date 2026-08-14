#pragma once

#include "command/library_callbacks.h"

namespace atm_desktop {

class ViewModel;

class IPanel : public atm::LibraryCallbacks {
public:
    virtual ~IPanel() = default;
    virtual const char* get_name() const = 0;
    virtual void draw(ViewModel& vm) = 0;
    virtual bool is_open() const { return open_; }
    virtual void set_open(bool open) { open_ = open; }

protected:
    bool open_ = true;
};

} // namespace atm_desktop

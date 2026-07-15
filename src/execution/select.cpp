#include "select.h"
#include <utility>

namespace exec {

    Select::Select(Iterator* child, Pred pred)
        : child_(child), pred_(std::move(pred)) {}

    void Select::open() { if (child_) child_->open(); }

    bool Select::next(std::vector<uint8_t>& out) {
        while (child_ && child_->next(out)) {
            if (pred_(out)) return true;
        }
        return false;
    }

    void Select::close() { if (child_) child_->close(); }

    Select::~Select() { /* no ownership of child */ }

}

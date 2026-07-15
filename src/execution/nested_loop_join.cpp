#include "nested_loop_join.h"

namespace exec {

    NestedLoopJoin::NestedLoopJoin(Iterator* outer, Iterator* inner, JoinPred pred)
        : outer_(outer), inner_(inner), pred_(pred) {}

    void NestedLoopJoin::open() {
        innerMaterialized_.clear();
        // Materializar inner
        if (inner_) {
            inner_->open();
            std::vector<uint8_t> rec;
            while (inner_->next(rec)) {
                innerMaterialized_.push_back(rec);
            }
            inner_->close();
        }
        if (outer_) outer_->open();
        outerExhausted_ = false;
        innerIdx_ = 0;
        currentOuter_.clear();
    }

    bool NestedLoopJoin::next(std::vector<uint8_t>& out) {
        // If we don't have current outer, fetch next
        while (true) {
            if (currentOuter_.empty()) {
                if (!outer_ || !outer_->next(currentOuter_)) {
                    return false;
                }
                innerIdx_ = 0;
            }

            while (innerIdx_ < innerMaterialized_.size()) {
                const auto& innerRec = innerMaterialized_[innerIdx_++];
                if (pred_(currentOuter_, innerRec)) {
                    // concatenate left|right
                    out = currentOuter_;
                    out.push_back('|');
                    out.insert(out.end(), innerRec.begin(), innerRec.end());
                    return true;
                }
            }

            // exhausted inner for current outer, clear currentOuter to fetch next
            currentOuter_.clear();
        }
    }

    void NestedLoopJoin::close() {
        if (outer_) outer_->close();
        innerMaterialized_.clear();
        currentOuter_.clear();
    }

    NestedLoopJoin::~NestedLoopJoin() { close(); }

}

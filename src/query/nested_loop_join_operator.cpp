#include "nested_loop_join_operator.h"

namespace query {

    NestedLoopJoinOperator::NestedLoopJoinOperator(Iterator* left,
                                                   Iterator* right,
                                                   JoinCondition joinCondition)
        : left_(left)
        , right_(right)
        , joinCondition_(std::move(joinCondition))
        , leftIndex_(0)
        , rightHasMore_(false)
        , done_(false)
    {}

    void NestedLoopJoinOperator::Open() {
        done_ = false;
        leftBuffer_.clear();
        leftIndex_ = 0;
        left_->Open();
        Record rec;
        while (left_->Next(rec)) {
            leftBuffer_.push_back(rec);
        }
        left_->Close();

        if (leftBuffer_.empty()) {
            done_ = true;
            return;
        }
        right_->Open();
        rightHasMore_ = right_->Next(rightCurrent_);

        if (!rightHasMore_) {
            right_->Close();
            done_ = true;
        }
    }

    bool NestedLoopJoinOperator::Next(Record& record) {

        while (!done_) {
            if (rightHasMore_) {
                const Record& leftRec = leftBuffer_[leftIndex_];

                bool matches = (joinCondition_ == nullptr)
                               || joinCondition_(leftRec, rightCurrent_);
                Record nextRight;
                bool hasNext = right_->Next(nextRight);

                if (matches) {
                    record.pageId = leftRec.pageId;
                    record.slotId = leftRec.slotId;
                    record.data.clear();
                    record.data.insert(record.data.end(),
                                       leftRec.data.begin(), leftRec.data.end());
                    record.data.insert(record.data.end(),
                                       rightCurrent_.data.begin(), rightCurrent_.data.end());
                    if (hasNext) {
                        rightCurrent_ = std::move(nextRight);
                        rightHasMore_ = true;
                    } else {
                        rightHasMore_ = false;
                    }
                    return true;
                }

                if (hasNext) {
                    rightCurrent_ = std::move(nextRight);
                    rightHasMore_ = true;
                } else {
                    rightHasMore_ = false;
                }
                continue;
            }

            right_->Close();
            leftIndex_++;

            if (leftIndex_ >= leftBuffer_.size()) {
                done_ = true;
                return false;
            }

            right_->Open();
            rightHasMore_ = right_->Next(rightCurrent_);

            if (!rightHasMore_) {
                right_->Close();
                done_ = true;
                return false;
            }
        }

        return false;
    }

    void NestedLoopJoinOperator::Close() {
        if (!done_) {
            right_->Close();
        }
        leftBuffer_.clear();
        done_ = true;
    }

} // namespace query

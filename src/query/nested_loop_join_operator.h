#pragma once

#include "iterator.h"
#include "record.h"
#include <vector>
#include <functional>

namespace query {
    class NestedLoopJoinOperator : public Iterator {
    public:
        using JoinCondition = std::function<bool(const Record&, const Record&)>;

        /**
         * @param left
         * @param right
         * @param joinCondition
         */
        NestedLoopJoinOperator(Iterator* left,
                               Iterator* right,
                               JoinCondition joinCondition = nullptr);

        void Open()               override;
        bool Next(Record& record) override;
        void Close()              override;

    private:
        Iterator*     left_;
        Iterator*     right_;
        JoinCondition joinCondition_;
        std::vector<Record> leftBuffer_;
        size_t leftIndex_;

        Record rightCurrent_;
        bool   rightHasMore_;

        bool done_;
        void ResetRight();
    };

} // namespace query

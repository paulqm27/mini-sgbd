#include "select_operator.h"

namespace query {

    SelectOperator::SelectOperator(Iterator* child, Predicate predicate)
        : child_(child)
        , predicate_(std::move(predicate))
    {}

    void SelectOperator::Open() {
        child_->Open();
    }

    bool SelectOperator::Next(Record& record) {
        Record candidate;
        while (child_->Next(candidate)) {
            if (predicate_(candidate)) {
                record = std::move(candidate);
                return true;
            }
        }
        return false;
    }

    void SelectOperator::Close() {
        child_->Close();
    }

} // namespace query

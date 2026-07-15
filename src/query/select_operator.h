#pragma once

#include "iterator.h"
#include "record.h"
#include <functional>
#include <memory>

namespace query {
    class SelectOperator : public Iterator {
    public:
        using Predicate = std::function<bool(const Record&)>;

        /**
         * @param child
         * @param predicate
         */
        SelectOperator(Iterator* child, Predicate predicate);

        void Open()               override;
        bool Next(Record& record) override;
        void Close()              override;

    private:
        Iterator*  child_;
        Predicate  predicate_;
    };

} // namespace query

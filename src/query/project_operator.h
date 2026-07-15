#pragma once

#include "iterator.h"
#include "record.h"
#include <vector>
#include <string>

namespace query {
    struct ColumnDef {
        std::string name;
        int offset;
        int size;
    };
    class ProjectOperator : public Iterator {
    public:
        /**
         * @param child
         * @param columns
         */
        ProjectOperator(Iterator* child, std::vector<ColumnDef> columns);

        void Open()               override;
        bool Next(Record& record) override;
        void Close()              override;

        const std::vector<ColumnDef>& GetColumns() const { return columns_; }

    private:
        Iterator*             child_;
        std::vector<ColumnDef> columns_;

        std::vector<uint8_t> Project(const std::vector<uint8_t>& source) const;
    };

} // namespace query

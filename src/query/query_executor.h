#pragma once

#include "record.h"
#include "iterator.h"
#include "project_operator.h"
#include "buffer/buffer.h"
#include "storage/storage.h"
#include "index/bplus_tree.h"

#include <functional>
#include <vector>
#include <string>
#include <memory>
#include <optional>

namespace query {
    struct Predicate {
        enum class Type {
            NONE,
            EQ_ID,
            GENERAL
        };

        Type type = Type::NONE;
        int  idValue = 0;
        std::function<bool(const Record&)> generalFn;

        static Predicate None()   { return Predicate{ Type::NONE, 0, nullptr }; }
        static Predicate EqId(int id) { return Predicate{ Type::EQ_ID, id, nullptr }; }
        static Predicate General(std::function<bool(const Record&)> fn) {
            Predicate p;
            p.type      = Type::GENERAL;
            p.generalFn = std::move(fn);
            return p;
        }
    };
    struct QueryPlan {
        Predicate              predicate  = Predicate::None();
        std::vector<ColumnDef> columns;
        int                    startPage  = 0;
    };

    struct QueryResult {
        std::vector<Record> records;
        int                 totalScanned = 0;
        double              hitRate      = 0.0;
        int                 pageAccesses = 0;
    };
    class QueryExecutor {
    public:
        /**
         * @param bufferManager
         * @param storageManager
         * @param bPlusTree
         */
        QueryExecutor(buffer::BufferManager*   bufferManager,
                      storage::StorageManager* storageManager,
                      index_m::BPlusTree*      bPlusTree = nullptr);

        /**
         *
         *
         * @param plan
         * @return
         */
        QueryResult Execute(const QueryPlan& plan);

        void Explain(const QueryPlan& plan) const;

    private:
        buffer::BufferManager*   bufferManager_;
        storage::StorageManager* storageManager_;
        index_m::BPlusTree*      bPlusTree_;

        bool CanUseIndex(const QueryPlan& plan) const;

        std::unique_ptr<Iterator> BuildPlan(const QueryPlan& plan) const;
    };

} // namespace query

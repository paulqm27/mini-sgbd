#pragma once

#include "iterator.h"
#include "record.h"
#include "index/bplus_tree.h"
#include "buffer/buffer.h"

namespace query {
    class IndexScanOperator : public Iterator {
    public:
        /**
         * @param tree
         * @param bufferManager
         * @param searchKey
         */
        IndexScanOperator(index_m::BPlusTree*   tree,
                          buffer::BufferManager* bufferManager,
                          int searchKey);

        void Open()               override;
        bool Next(Record& record) override;
        void Close()              override;

    private:
        index_m::BPlusTree*    tree_;
        buffer::BufferManager* bufferManager_;
        int                    searchKey_;

        index_m::RID rid_;
        bool         found_;
        bool         consumed_;
    };

} // namespace query

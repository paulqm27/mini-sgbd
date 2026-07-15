#pragma once

#include "iterator.h"
#include "record.h"
#include "buffer/buffer.h"
#include "storage/storage.h"

namespace query {
    class ScanOperator : public Iterator {
    public:
        /**
         * @param bufferManager
         * @param storageManager
         * @param startPage
         */
        ScanOperator(buffer::BufferManager* bufferManager,
                     storage::StorageManager* storageManager,
                     int startPage = 0);

        void Open()               override;
        bool Next(Record& record) override;
        void Close()              override;

    private:
        buffer::BufferManager*   bufferManager_;
        storage::StorageManager* storageManager_;

        int startPage_;
        int totalPages_;
        int currentPage_;
        int currentSlot_;
        int slotsInPage_;

        bool done_;
        bool AdvanceToNextSlot();
        void ReleaseCurrentPage();
    };

} // namespace query

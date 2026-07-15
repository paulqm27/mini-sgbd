#include "scan_operator.h"
#include <stdexcept>

namespace query {

    ScanOperator::ScanOperator(buffer::BufferManager* bufferManager,
                               storage::StorageManager* storageManager,
                               int startPage)
        : bufferManager_(bufferManager)
        , storageManager_(storageManager)
        , startPage_(startPage)
        , totalPages_(0)
        , currentPage_(-1)
        , currentSlot_(-1)
        , slotsInPage_(0)
        , done_(false)
    {}

    void ScanOperator::Open() {
        totalPages_ = storageManager_->GetNumPages();
        currentPage_ = startPage_ - 1; // Se incrementa en AdvanceToNextSlot
        currentSlot_ = -1;
        slotsInPage_ = 0;
        done_        = (totalPages_ <= startPage_);

        if (!done_) {
            currentPage_ = startPage_;
            auto* frame = bufferManager_->GetPage(currentPage_);
            slotsInPage_ = frame->page->GetNumSlots();
            bufferManager_->ReleasePage(currentPage_, false);

            currentSlot_ = -1;
        }
    }

    bool ScanOperator::Next(Record& record) {
        if (done_) return false;

        currentSlot_++;

        while (currentSlot_ >= slotsInPage_) {
            currentPage_++;
            if (currentPage_ >= totalPages_) {
                done_ = true;
                return false;
            }
            auto* frame = bufferManager_->GetPage(currentPage_);
            slotsInPage_ = frame->page->GetNumSlots();
            bufferManager_->ReleasePage(currentPage_, false);
            currentSlot_ = 0;
        }

        auto* frame = bufferManager_->GetPage(currentPage_);
        std::vector<uint8_t> data = frame->page->ReadRecord(currentSlot_);
        bufferManager_->ReleasePage(currentPage_, false);

        if (data.empty()) {
            return Next(record);
        }

        record.pageId = currentPage_;
        record.slotId = currentSlot_;
        record.data   = std::move(data);
        return true;
    }

    void ScanOperator::Close() {
        done_ = true;
    }

} // namespace query

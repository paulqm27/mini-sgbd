#include "index_scan_operator.h"

namespace query {

    IndexScanOperator::IndexScanOperator(index_m::BPlusTree*   tree,
                                         buffer::BufferManager* bufferManager,
                                         int searchKey)
        : tree_(tree)
        , bufferManager_(bufferManager)
        , searchKey_(searchKey)
        , found_(false)
        , consumed_(false)
    {}

    void IndexScanOperator::Open() {
        consumed_ = false;
        rid_   = tree_->Search(searchKey_);
        found_ = rid_.IsValid();
    }

    bool IndexScanOperator::Next(Record& record) {
        if (!found_ || consumed_) {
            return false;
        }
        consumed_ = true;
        auto* frame = bufferManager_->GetPage(rid_.pageId);
        if (!frame) {
            return false;
        }

        std::vector<uint8_t> data = frame->page->ReadRecord(rid_.slotId);

        bufferManager_->ReleasePage(rid_.pageId, false);

        if (data.empty()) {
            return false;
        }

        record.pageId = rid_.pageId;
        record.slotId = rid_.slotId;
        record.data   = std::move(data);
        return true;
    }

    void IndexScanOperator::Close() {
        consumed_ = true;
        found_    = false;
    }

} // namespace query

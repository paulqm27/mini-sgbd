#include "index_scan.h"
#include "../buffer/buffer.h"

using namespace std;

namespace exec {

    IndexScan::IndexScan(index_m::BPlusTree* tree, buffer::BufferManager* bm, int key)
        : tree_(tree), bm_(bm), key_(key), returned_(false) {}

    void IndexScan::open() { returned_ = false; }

    bool IndexScan::next(vector<uint8_t>& out) {
        if (returned_) return false;
        if (!tree_) return false;
        index_m::RID rid = tree_->Search(key_);
        if (!rid.IsValid()) return false;
        auto frame = bm_->GetPage(rid.pageId);
        if (!frame || !frame->page) return false;
        out = frame->page->ReadRecord(rid.slotId);
        bm_->ReleasePage(rid.pageId, false);
        returned_ = true;
        return true;
    }

    void IndexScan::close() { returned_ = false; }

    IndexScan::~IndexScan() { close(); }

}

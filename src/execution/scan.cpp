#include "scan.h"
#include "../buffer/buffer.h"
#include "../storage/page.h"

#include <iostream>

using namespace std;

namespace exec {

    Scan::Scan(buffer::BufferManager* bm, int startPage, int endPage)
        : bm_(bm), startPage_(startPage), endPage_(endPage) {}

    void Scan::open() {
        currentPage_ = startPage_;
        currentRecords_.clear();
        recordIdx_ = 0;
        currentFrame_ = nullptr;
    }

    bool Scan::next(vector<uint8_t>& out) {
        while (true) {
            if (recordIdx_ < currentRecords_.size()) {
                out = currentRecords_[recordIdx_++];
                return true;
            }

            if (currentFrame_) {
                bm_->ReleasePage(currentPage_, false);
                currentFrame_ = nullptr;
            }

            if (currentPage_ > endPage_) return false;

            currentFrame_ = bm_->GetPage(currentPage_);
            if (!currentFrame_ || !currentFrame_->page) {
                // página vacía o inexistente
                if (currentFrame_) bm_->ReleasePage(currentPage_, false);
                currentPage_++;
                continue;
            }

            currentRecords_ = currentFrame_->page->ReadAllRecords();
            recordIdx_ = 0;
            currentPage_++;
        }
    }

    void Scan::close() {
        if (currentFrame_) {
            bm_->ReleasePage(currentPage_ - 1, false);
            currentFrame_ = nullptr;
        }
        currentRecords_.clear();
        recordIdx_ = 0;
    }

    Scan::~Scan() { close(); }

}

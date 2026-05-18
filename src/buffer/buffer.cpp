#include "buffer.h"
#include <algorithm>

namespace buffer {

BufferManager::BufferManager(int capacity, storage::StorageManager* storageManager)
    : capacity_(capacity), storageManager_(storageManager) {
}

BufferManager::~BufferManager() {
    Flush();
}

void BufferManager::UpdateLRU(int pageId) {
    lruList_.remove(pageId);
    lruList_.push_back(pageId);
}

void BufferManager::ReplacePage() {
    for (auto it = lruList_.begin(); it != lruList_.end(); ++it) {
        int pageId = *it;
        auto& frame = frames_[pageId];

        if (frame->pinCount == 0) {
            if (frame->dirty) {
                storageManager_->WritePageData(pageId, *(frame->page));
            }
            frames_.erase(pageId);
            lruList_.erase(it);
            return;
        }
    }
}

Frame* BufferManager::GetPage(int pageId) {
    auto it = frames_.find(pageId);
    if (it != frames_.end()) {
        it->second->pinCount++;
        UpdateLRU(pageId);
        return it->second.get();
    }

    if (static_cast<int>(frames_.size()) >= capacity_) {
        ReplacePage();
    }

    auto page = storageManager_->ReadPageData(pageId);
    if (!page) {
        page = std::make_unique<storage::Page>();
    }

    auto frame = std::make_unique<Frame>(pageId, std::move(page));
    frame->pinCount = 1;
    Frame* rawFrame = frame.get();
    
    frames_[pageId] = std::move(frame);
    UpdateLRU(pageId);

    return rawFrame;
}

void BufferManager::ReleasePage(int pageId, bool dirty) {
    auto it = frames_.find(pageId);
    if (it != frames_.end()) {
        it->second->pinCount--;
        if (dirty) {
            it->second->dirty = true;
        }
    }
}

void BufferManager::Flush() {
    for (auto& pair : frames_) {
        if (pair.second->dirty) {
            storageManager_->WritePageData(pair.first, *(pair.second->page));
            pair.second->dirty = false;
        }
    }
}

} // namespace buffer

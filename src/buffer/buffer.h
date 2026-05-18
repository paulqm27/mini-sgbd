#pragma once

#include "storage/storage.h"
#include "storage/page.h"
#include <unordered_map>
#include <list>
#include <memory>

namespace buffer {

struct Frame {
    int pageId;
    std::unique_ptr<storage::Page> page;
    int pinCount;
    bool dirty;

    Frame(int id, std::unique_ptr<storage::Page> p) 
        : pageId(id), page(std::move(p)), pinCount(0), dirty(false) {}
};

class BufferManager {
public:
    BufferManager(int capacity, storage::StorageManager* storageManager);
    ~BufferManager();

    Frame* GetPage(int pageId);
    void ReleasePage(int pageId, bool dirty);
    void Flush();

private:
    int capacity_;
    storage::StorageManager* storageManager_;
    std::unordered_map<int, std::unique_ptr<Frame>> frames_;
    std::list<int> lruList_;

    void UpdateLRU(int pageId);
    void ReplacePage();
};

} // namespace buffer

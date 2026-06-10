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

        // Estadísticas y visualización
        int GetAccessCount() const { return numAccesses_; }
        int GetHitCount() const { return numHits_; }
        int GetMissCount() const { return numMisses_; }
        double GetHitRate() const {
            if (numAccesses_ == 0) return 0.0;
            return static_cast<double>(numHits_) / numAccesses_;
        }
        void ResetStats() {
            numAccesses_ = 0;
            numHits_ = 0;
            numMisses_ = 0;
        }
        void PrintStatus() const;

    private:
        int capacity_;
        storage::StorageManager* storageManager_;
        std::unordered_map<int, std::unique_ptr<Frame>> frames_;
        std::list<int> lruList_;
        void UpdateLRU(int pageId);
        bool ReplacePage();

        int numAccesses_ = 0;
        int numHits_ = 0;
        int numMisses_ = 0;
    };
}

#include "buffer.h"
#include <algorithm>
#include <iostream>

namespace buffer {

    BufferManager::BufferManager(int capacity, storage::StorageManager* storageManager)
        : capacity_(capacity), storageManager_(storageManager), numAccesses_(0), numHits_(0), numMisses_(0) {
    }

    BufferManager::~BufferManager() {
        Flush();
    }

    void BufferManager::UpdateLRU(int pageId) {
        lruList_.remove(pageId);
        lruList_.push_back(pageId);
    }

    bool BufferManager::ReplacePage() {
        for (auto it = lruList_.begin(); it != lruList_.end(); ++it) {
            int pageId = *it;
            auto& frame = frames_[pageId];
            if (frame->pinCount == 0) {
                if (frame->dirty) {
                    storageManager_->WritePageData(pageId, *(frame->page));
                }
                frames_.erase(pageId);
                lruList_.erase(it);
                return true; // Evicción exitosa
            }
        }
        return false; // No se pudo evictar ninguna página porque todas están pinned
    }

    Frame* BufferManager::GetPage(int pageId) {
        numAccesses_++;
        auto it = frames_.find(pageId);
        if (it != frames_.end()) {
            numHits_++;
            it->second->pinCount++;
            UpdateLRU(pageId);
            return it->second.get();
        }
        numMisses_++;
        while (static_cast<int>(frames_.size()) >= capacity_) {
            if (!ReplacePage()) {
                break; // Evitar bucle infinito si todas están ocupadas (pinned)
            }
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

    void BufferManager::PrintStatus() const {
        std::cout << "\n--- ESTADO DEL BUFFER POOL ---" << std::endl;
        std::cout << "Capacidad del Pool: " << capacity_ << " paginas" << std::endl;
        std::cout << "Paginas cargadas en memoria RAM (" << frames_.size() << "):" << std::endl;
        for (const auto& pair : frames_) {
            const auto& frame = pair.second;
            std::cout << "  - Pagina " << frame->pageId 
                      << " [Pin Count: " << frame->pinCount 
                      << ", Dirty: " << (frame->dirty ? "SI" : "NO") << "]" << std::endl;
        }
        std::cout << "Lista LRU (Menos usado -> Mas usado): ";
        for (auto it = lruList_.begin(); it != lruList_.end(); ++it) {
            std::cout << *it;
            if (std::next(it) != lruList_.end()) std::cout << " -> ";
        }
        std::cout << std::endl;
        std::cout << "Estadisticas de Acceso:" << std::endl;
        std::cout << "  - Accesos Totales: " << numAccesses_ << std::endl;
        std::cout << "  - Hits: " << numHits_ << std::endl;
        std::cout << "  - Misses: " << numMisses_ << std::endl;
        std::cout << "  - Hit Rate: " << (GetHitRate() * 100.0) << "%" << std::endl;
        std::cout << "------------------------------\n" << std::endl;
    }
}

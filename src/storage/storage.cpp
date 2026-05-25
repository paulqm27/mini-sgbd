#include "storage.h"
#include <iostream>

namespace storage {

    StorageManager::StorageManager(const std::string& filename) : filename_(filename) {
        file_.open(filename, std::ios::in | std::ios::out | std::ios::binary);
        if (!file_.is_open()) {
            file_.clear();
            file_.open(filename, std::ios::out | std::ios::binary);
            file_.close();
            file_.open(filename, std::ios::in | std::ios::out | std::ios::binary);
        }
    }

    StorageManager::~StorageManager() {
        Close();
    }

    bool StorageManager::WritePage(int pageId, const std::vector<uint8_t>& data) {
        if (!file_.is_open()) return false;

        std::streampos pos = static_cast<std::streampos>(pageId * PAGE_SIZE);
        file_.seekp(pos);
        file_.write(reinterpret_cast<const char*>(data.data()), data.size());
        file_.flush();
        return file_.good();
    }

    std::vector<uint8_t> StorageManager::ReadPage(int pageId) {
        std::vector<uint8_t> data(PAGE_SIZE, 0);
        if (!file_.is_open()) return data;

        std::streampos pos = static_cast<std::streampos>(pageId * PAGE_SIZE);
        file_.seekg(pos);
        file_.read(reinterpret_cast<char*>(data.data()), PAGE_SIZE);

        if (file_.eof()) {
            file_.clear();
        }

        return data;
    }

    bool StorageManager::WritePageData(int pageId, const Page& page) {
        return WritePage(pageId, page.GetData());
    }

    std::unique_ptr<Page> StorageManager::ReadPageData(int pageId) {
        std::vector<uint8_t> data = ReadPage(pageId);
        return std::make_unique<Page>(data);
    }

    void StorageManager::Close() {
        if (file_.is_open()) {
            file_.close();
        }
    }

}

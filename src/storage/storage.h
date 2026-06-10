#pragma once

#include "page.h"
#include <string>
#include <fstream>
#include <memory>

namespace storage {

    class StorageManager {

    public:
        explicit StorageManager(const std::string& filename);
        ~StorageManager();

        bool WritePage(int pageId, const std::vector<uint8_t>& data);
        std::vector<uint8_t> ReadPage(int pageId);

        bool WritePageData(int pageId, const Page& page);
        std::unique_ptr<Page> ReadPageData(int pageId);

        void Close();
        int GetNumPages();

    private:
        std::string filename_;
        std::fstream file_;
        int numPages_ = 0;
    };

}

#pragma once

#include "iterator.h"
#include <vector>
#include <memory>
#include <cstddef>

namespace buffer { class BufferManager; struct Frame; }

namespace exec {

    class Scan : public Iterator {
    public:
        Scan(buffer::BufferManager* bm, int startPage, int endPage);
        void open() override;
        bool next(std::vector<uint8_t>& out) override;
        void close() override;
        ~Scan() override;

    private:
        buffer::BufferManager* bm_ = nullptr;
        int startPage_ = 1;
        int endPage_ = 1;
        int currentPage_ = -1;
        std::vector<std::vector<uint8_t>> currentRecords_;
        size_t recordIdx_ = 0;
        buffer::Frame* currentFrame_ = nullptr;
    };

}

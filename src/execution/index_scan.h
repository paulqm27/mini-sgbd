#pragma once

#include "iterator.h"
#include "../index/bplus_tree.h"
#include <vector>

namespace buffer { class BufferManager; }

namespace exec {

    // IndexScan: busca una clave exacta en un B+ Tree y retorna el registro asociado
    class IndexScan : public Iterator {
    public:
        IndexScan(index_m::BPlusTree* tree, buffer::BufferManager* bm, int key);
        void open() override;
        bool next(std::vector<uint8_t>& out) override;
        void close() override;
        ~IndexScan() override;

    private:
        index_m::BPlusTree* tree_ = nullptr;
        buffer::BufferManager* bm_ = nullptr;
        int key_ = 0;
        bool returned_ = false;
    };

}

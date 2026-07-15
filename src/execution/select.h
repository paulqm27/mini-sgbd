#pragma once

#include "iterator.h"
#include <functional>
#include <vector>

namespace exec {

    class Select : public Iterator {
    public:
        using Pred = std::function<bool(const std::vector<uint8_t>&)>;
        Select(Iterator* child, Pred pred);
        void open() override;
        bool next(std::vector<uint8_t>& out) override;
        void close() override;
        ~Select() override;

    private:
        Iterator* child_;
        Pred pred_;
    };

}

#pragma once

#include "iterator.h"
#include <vector>
#include <functional>

namespace exec {

    class NestedLoopJoin : public Iterator {
    public:
        using JoinPred = std::function<bool(const std::vector<uint8_t>&, const std::vector<uint8_t>&)>;
        NestedLoopJoin(Iterator* outer, Iterator* inner, JoinPred pred);
        void open() override;
        bool next(std::vector<uint8_t>& out) override;
        void close() override;
        ~NestedLoopJoin() override;

    private:
        Iterator* outer_;
        Iterator* inner_;
        JoinPred pred_;

        std::vector<std::vector<uint8_t>> innerMaterialized_;
        size_t outerIdx_ = 0;
        std::vector<uint8_t> currentOuter_;
        size_t innerIdx_ = 0;
        bool outerExhausted_ = false;
    };

}

#pragma once

#include "iterator.h"
#include <functional>
#include <vector>

namespace exec {

    class Project : public Iterator {
    public:
        using Proj = std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)>;
        Project(Iterator* child, Proj proj);
        void open() override;
        bool next(std::vector<uint8_t>& out) override;
        void close() override;
        ~Project() override;

    private:
        Iterator* child_;
        Proj proj_;
    };

}

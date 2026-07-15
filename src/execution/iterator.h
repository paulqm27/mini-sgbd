#pragma once

#include <vector>
#include <cstdint>

namespace exec {

    class Iterator {
    public:
        virtual void open() = 0;
        // next devuelve true y rellena `out` mientras haya tuplas
        virtual bool next(std::vector<uint8_t>& out) = 0;
        virtual void close() = 0;
        virtual ~Iterator() {}
    };

}

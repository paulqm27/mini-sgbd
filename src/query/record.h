#pragma once

#include <vector>
#include <cstdint>

namespace query {
    struct Record {
        int pageId = -1;
        int slotId = -1;
        std::vector<uint8_t> data;

        bool IsValid() const {
            return pageId >= 0 && slotId >= 0 && !data.empty();
        }
    };

} // namespace query

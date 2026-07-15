#pragma once

#include <vector>
#include <cstdint>

namespace query {

    /**
     * Record — Unidad de dato que fluye entre operadores en el modelo Volcano.
     *
     * Contiene:
     *   - pageId / slotId : posición física del registro en el Storage Manager
     *   - data            : bytes crudos del registro (misma representación que Page::ReadRecord)
     *
     * El campo 'data' es opaco para los operadores base; los operadores de
     * proyección e igualdad lo interpretan conociendo el esquema de la tabla.
     */
    struct Record {
        int pageId = -1;
        int slotId = -1;
        std::vector<uint8_t> data;

        bool IsValid() const {
            return pageId >= 0 && slotId >= 0 && !data.empty();
        }
    };

} // namespace query

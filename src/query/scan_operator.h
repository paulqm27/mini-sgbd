#pragma once

#include "iterator.h"
#include "record.h"
#include "buffer/buffer.h"
#include "storage/storage.h"

namespace query {

    /**
     * ScanOperator — Escaneo secuencial completo de tabla (Full Table Scan).
     *
     * Recorre todas las páginas del Storage Manager de forma secuencial,
     * leyendo cada slot válido dentro de cada página. Utiliza el Buffer Manager
     * para cargar una página a la vez — NO carga toda la tabla en memoria.
     *
     * Complejidad: O(N·P) donde N = número de páginas, P = slots por página.
     *
     * Integración con el sistema existente:
     *   - Usa BufferManager::GetPage(pageId) para obtener una página.
     *   - Usa BufferManager::ReleasePage(pageId, false) al terminar con cada página.
     *   - Usa StorageManager::GetNumPages() para saber cuántas páginas hay.
     *   - Usa Page::GetNumSlots() y Page::ReadRecord(slotId) para leer registros.
     */
    class ScanOperator : public Iterator {
    public:
        /**
         * @param bufferManager  Buffer Manager del sistema (ya existente).
         * @param storageManager Storage Manager del sistema (ya existente).
         * @param startPage      Primera página a escanear (por defecto 0).
         */
        ScanOperator(buffer::BufferManager* bufferManager,
                     storage::StorageManager* storageManager,
                     int startPage = 0);

        void Open()               override;
        bool Next(Record& record) override;
        void Close()              override;

    private:
        buffer::BufferManager*   bufferManager_;
        storage::StorageManager* storageManager_;

        int startPage_;      // Primera página a escanear
        int totalPages_;     // Total de páginas en disco (calculado en Open)
        int currentPage_;    // Página que se está leyendo ahora
        int currentSlot_;    // Slot que se está leyendo dentro de currentPage_
        int slotsInPage_;    // Total de slots de la página actual

        bool done_;          // true cuando se agotaron todos los registros

        // Avanza al siguiente slot válido (puede cambiar de página).
        // Devuelve false si no hay más registros.
        bool AdvanceToNextSlot();

        // Libera la página actual del buffer (si hay una en uso).
        void ReleaseCurrentPage();
    };

} // namespace query

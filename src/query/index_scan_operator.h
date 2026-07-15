#pragma once

#include "iterator.h"
#include "record.h"
#include "index/bplus_tree.h"
#include "buffer/buffer.h"

namespace query {

    /**
     * IndexScanOperator — Búsqueda puntual usando el índice B+ Tree existente.
     *
     * En lugar de recorrer toda la tabla (O(N·P)), usa el B+ Tree para obtener
     * directamente el RID {pageId, slotId} del registro con la clave buscada.
     * Complejidad: O(log N) para la búsqueda en el árbol + 1 acceso a disco.
     *
     * Ejemplo:
     *   SELECT * FROM Personas WHERE id = 100;
     *
     *   IndexScanOperator idxScan(&bPlusTree, &bufferManager, 100);
     *   idxScan.Open();
     *   Record r;
     *   if (idxScan.Next(r)) { ... }
     *   idxScan.Close();
     *
     * Notas:
     *   - El B+ Tree almacena exactamente un RID por clave (sin duplicados).
     *   - Si la clave no existe, Next() devuelve false en la primera llamada.
     *   - El operador emite como máximo UN registro por consulta.
     *   - El Buffer Manager gestiona el caching de la página leída.
     */
    class IndexScanOperator : public Iterator {
    public:
        /**
         * @param tree          Árbol B+ ya inicializado con el índice de la tabla.
         * @param bufferManager Buffer Manager del sistema.
         * @param searchKey     Clave entera a buscar.
         */
        IndexScanOperator(index_m::BPlusTree*   tree,
                          buffer::BufferManager* bufferManager,
                          int searchKey);

        void Open()               override;
        bool Next(Record& record) override;
        void Close()              override;

    private:
        index_m::BPlusTree*    tree_;
        buffer::BufferManager* bufferManager_;
        int                    searchKey_;

        index_m::RID rid_;        // RID encontrado por el B+ Tree
        bool         found_;      // true si la clave existe en el índice
        bool         consumed_;   // true si Next() ya devolvió el único resultado
    };

} // namespace query

#pragma once

#include "iterator.h"
#include "record.h"
#include <vector>
#include <functional>

namespace query {

    /**
     * NestedLoopJoinOperator — Join mediante bucles anidados (Simple NLJ).
     *
     * Implementa el algoritmo de Nested Loop Join clásico:
     *
     *   for each r in Left:
     *       for each s in Right:
     *           if join_condition(r, s):
     *               output(r ++ s)       // concatenación de datos
     *
     * Estrategia de materialización:
     *   - En Open(), todos los registros del lado IZQUIERDO se leen y almacenan
     *     en un buffer en memoria (leftBuffer_). Esto es correcto para el Simple NLJ.
     *   - El lado DERECHO se relee desde su Open() para cada registro izquierdo.
     *     El buffer del sistema gestiona el caching automáticamente.
     *
     * El registro resultante tiene data = left.data + right.data (concatenación).
     * pageId / slotId corresponden al registro izquierdo (el "driver").
     *
     * Condición de join:
     *   - Por defecto (joinCondition = nullptr) se realiza un CROSS JOIN.
     *   - Si se proporciona una función, solo se emiten pares que la satisfagan.
     *
     * Ejemplo de uso (join natural por id):
     *   auto join = std::make_unique<NestedLoopJoinOperator>(
     *       &scanPersonas,
     *       &scanDepartamentos,
     *       [](const Record& l, const Record& r) {
     *           int32_t idL, idR;
     *           std::memcpy(&idL, l.data.data(), 4);
     *           std::memcpy(&idR, r.data.data(), 4);
     *           return idL == idR;
     *       }
     *   );
     */
    class NestedLoopJoinOperator : public Iterator {
    public:
        using JoinCondition = std::function<bool(const Record&, const Record&)>;

        /**
         * @param left          Iterador del lado izquierdo (driver).
         * @param right         Iterador del lado derecho (inner).
         * @param joinCondition Predicado opcional. Si es nullptr: Cross Join.
         */
        NestedLoopJoinOperator(Iterator* left,
                               Iterator* right,
                               JoinCondition joinCondition = nullptr);

        void Open()               override;
        bool Next(Record& record) override;
        void Close()              override;

    private:
        Iterator*     left_;
        Iterator*     right_;
        JoinCondition joinCondition_;

        // Buffer que materializa todos los registros del lado izquierdo.
        std::vector<Record> leftBuffer_;
        size_t leftIndex_;    // Índice actual en leftBuffer_

        // Registro derecho activo en la iteración interna.
        Record rightCurrent_;
        bool   rightHasMore_; // false cuando el iterador derecho se agotó

        bool done_;

        // Reinicia el iterador derecho para un nuevo ciclo de la iteración interna.
        void ResetRight();
    };

} // namespace query

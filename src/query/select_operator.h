#pragma once

#include "iterator.h"
#include "record.h"
#include <functional>
#include <memory>

namespace query {

    /**
     * SelectOperator — Filtrado de registros mediante predicado (σ en álgebra relacional).
     *
     * Recibe un iterador hijo y un predicado booleano. En cada llamada a Next(),
     * consume registros del hijo hasta encontrar uno que cumpla el predicado.
     *
     * Ejemplo de uso:
     *   // Filtrar personas con edad > 18
     *   auto scan   = std::make_unique<ScanOperator>(&bm, &sm);
     *   auto select = std::make_unique<SelectOperator>(
     *       scan.get(),
     *       [](const Record& r) {
     *           int32_t edad;
     *           std::memcpy(&edad, r.data.data() + 4, 4); // offset 4 = campo edad
     *           return edad > 18;
     *       }
     *   );
     *
     * El SelectOperator NO posee el iterador hijo; el llamador es responsable
     * de su ciclo de vida.
     */
    class SelectOperator : public Iterator {
    public:
        using Predicate = std::function<bool(const Record&)>;

        /**
         * @param child     Iterador que produce los registros a filtrar.
         * @param predicate Función que devuelve true si el registro pasa el filtro.
         */
        SelectOperator(Iterator* child, Predicate predicate);

        void Open()               override;
        bool Next(Record& record) override;
        void Close()              override;

    private:
        Iterator*  child_;
        Predicate  predicate_;
    };

} // namespace query

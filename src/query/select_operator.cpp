#include "select_operator.h"

namespace query {

    SelectOperator::SelectOperator(Iterator* child, Predicate predicate)
        : child_(child)
        , predicate_(std::move(predicate))
    {}

    void SelectOperator::Open() {
        // Delegar la apertura al iterador hijo.
        child_->Open();
    }

    bool SelectOperator::Next(Record& record) {
        // Consumir registros del hijo hasta encontrar uno que pase el predicado.
        Record candidate;
        while (child_->Next(candidate)) {
            if (predicate_(candidate)) {
                record = std::move(candidate);
                return true;
            }
            // El candidato no pasó el filtro: descartar y pedir el siguiente.
        }
        // El hijo se agotó sin encontrar un registro que pase el filtro.
        return false;
    }

    void SelectOperator::Close() {
        child_->Close();
    }

} // namespace query

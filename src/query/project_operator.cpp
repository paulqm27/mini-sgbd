#include "project_operator.h"
#include <cstring>
#include <stdexcept>

namespace query {

    ProjectOperator::ProjectOperator(Iterator* child, std::vector<ColumnDef> columns)
        : child_(child)
        , columns_(std::move(columns))
    {}

    void ProjectOperator::Open() {
        child_->Open();
    }

    bool ProjectOperator::Next(Record& record) {
        Record source;
        if (!child_->Next(source)) {
            return false;
        }

        // Mantener pageId/slotId originales para trazabilidad.
        record.pageId = source.pageId;
        record.slotId = source.slotId;
        record.data   = Project(source.data);
        return true;
    }

    void ProjectOperator::Close() {
        child_->Close();
    }

    std::vector<uint8_t> ProjectOperator::Project(const std::vector<uint8_t>& source) const {
        // Calcular el tamaño total del resultado proyectado.
        size_t totalSize = 0;
        for (const auto& col : columns_) {
            totalSize += static_cast<size_t>(col.size);
        }

        std::vector<uint8_t> result(totalSize, 0);
        size_t destOffset = 0;

        for (const auto& col : columns_) {
            size_t srcEnd = static_cast<size_t>(col.offset) + static_cast<size_t>(col.size);
            if (srcEnd <= source.size()) {
                std::memcpy(result.data() + destOffset,
                            source.data() + col.offset,
                            static_cast<size_t>(col.size));
            }
            // Si el registro fuente es más corto de lo esperado, se dejan ceros.
            destOffset += static_cast<size_t>(col.size);
        }

        return result;
    }

} // namespace query

#include "index_scan_operator.h"

namespace query {

    IndexScanOperator::IndexScanOperator(index_m::BPlusTree*   tree,
                                         buffer::BufferManager* bufferManager,
                                         int searchKey)
        : tree_(tree)
        , bufferManager_(bufferManager)
        , searchKey_(searchKey)
        , found_(false)
        , consumed_(false)
    {}

    void IndexScanOperator::Open() {
        consumed_ = false;

        // --- Paso 1: Buscar en el B+ Tree ---
        // BPlusTree::Search() recorre el árbol en O(log N) y devuelve el RID
        // del registro, o un RID inválido si la clave no existe.
        rid_   = tree_->Search(searchKey_);
        found_ = rid_.IsValid();
    }

    bool IndexScanOperator::Next(Record& record) {
        // Este operador emite como máximo UN registro.
        if (!found_ || consumed_) {
            return false;
        }
        consumed_ = true;

        // --- Paso 2: Leer la página física usando el Buffer Manager ---
        // El BufferManager hace el pin de la página; debemos hacer ReleasePage.
        auto* frame = bufferManager_->GetPage(rid_.pageId);
        if (!frame) {
            return false;
        }

        // Leer el slot exacto apuntado por el RID.
        std::vector<uint8_t> data = frame->page->ReadRecord(rid_.slotId);

        // Liberar la página (sin marcarla como dirty, es solo lectura).
        bufferManager_->ReleasePage(rid_.pageId, false);

        if (data.empty()) {
            return false;
        }

        record.pageId = rid_.pageId;
        record.slotId = rid_.slotId;
        record.data   = std::move(data);
        return true;
    }

    void IndexScanOperator::Close() {
        consumed_ = true;
        found_    = false;
    }

} // namespace query

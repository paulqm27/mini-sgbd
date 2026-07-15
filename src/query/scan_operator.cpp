#include "scan_operator.h"
#include <stdexcept>

namespace query {

    ScanOperator::ScanOperator(buffer::BufferManager* bufferManager,
                               storage::StorageManager* storageManager,
                               int startPage)
        : bufferManager_(bufferManager)
        , storageManager_(storageManager)
        , startPage_(startPage)
        , totalPages_(0)
        , currentPage_(-1)
        , currentSlot_(-1)
        , slotsInPage_(0)
        , done_(false)
    {}

    void ScanOperator::Open() {
        // Calcular cuántas páginas existen actualmente en disco.
        totalPages_ = storageManager_->GetNumPages();

        // Inicializar cursores al punto de partida.
        currentPage_ = startPage_ - 1; // Se incrementa en AdvanceToNextSlot
        currentSlot_ = -1;
        slotsInPage_ = 0;
        done_        = (totalPages_ <= startPage_);

        if (!done_) {
            // Posicionarse en el primer slot válido.
            // Incrementamos currentPage_ para que quede en startPage_.
            currentPage_ = startPage_;
            // Cargar la primera página para obtener slotsInPage_.
            auto* frame = bufferManager_->GetPage(currentPage_);
            slotsInPage_ = frame->page->GetNumSlots();
            bufferManager_->ReleasePage(currentPage_, false);

            // Retroceder el slot para que el primer Next() funcione correctamente.
            currentSlot_ = -1;
        }
    }

    bool ScanOperator::Next(Record& record) {
        if (done_) return false;

        // Avanzar al siguiente slot.
        currentSlot_++;

        // Si superamos los slots de la página actual, avanzar a la siguiente.
        while (currentSlot_ >= slotsInPage_) {
            currentPage_++;
            if (currentPage_ >= totalPages_) {
                done_ = true;
                return false;
            }
            // Cargar la nueva página y actualizar slotsInPage_.
            auto* frame = bufferManager_->GetPage(currentPage_);
            slotsInPage_ = frame->page->GetNumSlots();
            bufferManager_->ReleasePage(currentPage_, false);
            currentSlot_ = 0;
        }

        // Leer el registro del slot actual.
        auto* frame = bufferManager_->GetPage(currentPage_);
        std::vector<uint8_t> data = frame->page->ReadRecord(currentSlot_);
        bufferManager_->ReleasePage(currentPage_, false);

        // Saltar registros vacíos (tamaño 0).
        if (data.empty()) {
            return Next(record);
        }

        record.pageId = currentPage_;
        record.slotId = currentSlot_;
        record.data   = std::move(data);
        return true;
    }

    void ScanOperator::Close() {
        done_ = true;
        // No hay recursos pendientes que liberar explícitamente:
        // cada GetPage fue emparejado con su ReleasePage dentro de Next/Open.
    }

} // namespace query

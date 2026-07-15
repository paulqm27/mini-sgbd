#include "nested_loop_join_operator.h"

namespace query {

    NestedLoopJoinOperator::NestedLoopJoinOperator(Iterator* left,
                                                   Iterator* right,
                                                   JoinCondition joinCondition)
        : left_(left)
        , right_(right)
        , joinCondition_(std::move(joinCondition))
        , leftIndex_(0)
        , rightHasMore_(false)
        , done_(false)
    {}

    void NestedLoopJoinOperator::Open() {
        done_ = false;
        leftBuffer_.clear();
        leftIndex_ = 0;

        // --- Materializar el lado izquierdo en memoria ---
        // Se hace una sola vez en Open(). Desde aquí en adelante, el buffer
        // sirve como fuente infinitamente rereíble sin tocar el disco/buffer pool.
        left_->Open();
        Record rec;
        while (left_->Next(rec)) {
            leftBuffer_.push_back(rec);
        }
        left_->Close();

        if (leftBuffer_.empty()) {
            done_ = true;
            return;
        }

        // Abrir el iterador derecho para la primera pasada.
        right_->Open();
        rightHasMore_ = right_->Next(rightCurrent_);

        if (!rightHasMore_) {
            // El lado derecho está vacío: no hay ningún par posible.
            right_->Close();
            done_ = true;
        }
    }

    bool NestedLoopJoinOperator::Next(Record& record) {
        // Esquema del algoritmo:
        //   leftIndex_ avanza por leftBuffer_.
        //   Para cada registro izquierdo, agotamos el iterador derecho.
        //   Cuando el derecho se agota, avanzamos leftIndex_ y reiniciamos el derecho.

        while (!done_) {
            // Verificar si el par actual (leftIndex_, rightCurrent_) satisface la condición.
            if (rightHasMore_) {
                const Record& leftRec = leftBuffer_[leftIndex_];

                bool matches = (joinCondition_ == nullptr)
                               || joinCondition_(leftRec, rightCurrent_);

                // Avanzar el iterador derecho ANTES de emitir, para la próxima llamada.
                Record nextRight;
                bool hasNext = right_->Next(nextRight);

                if (matches) {
                    // Construir el registro de salida: concatenar datos L ++ R.
                    record.pageId = leftRec.pageId;
                    record.slotId = leftRec.slotId;
                    record.data.clear();
                    record.data.insert(record.data.end(),
                                       leftRec.data.begin(), leftRec.data.end());
                    record.data.insert(record.data.end(),
                                       rightCurrent_.data.begin(), rightCurrent_.data.end());

                    // Actualizar el estado del derecho para la siguiente llamada.
                    if (hasNext) {
                        rightCurrent_ = std::move(nextRight);
                        rightHasMore_ = true;
                    } else {
                        rightHasMore_ = false;
                    }
                    return true;
                }

                // No coincide: avanzar el estado del derecho.
                if (hasNext) {
                    rightCurrent_ = std::move(nextRight);
                    rightHasMore_ = true;
                } else {
                    rightHasMore_ = false;
                }
                continue;
            }

            // El iterador derecho se agotó: avanzar al siguiente registro izquierdo.
            right_->Close();
            leftIndex_++;

            if (leftIndex_ >= leftBuffer_.size()) {
                done_ = true;
                return false;
            }

            // Reiniciar el iterador derecho para el nuevo registro izquierdo.
            right_->Open();
            rightHasMore_ = right_->Next(rightCurrent_);

            if (!rightHasMore_) {
                right_->Close();
                done_ = true;
                return false;
            }
        }

        return false;
    }

    void NestedLoopJoinOperator::Close() {
        if (!done_) {
            right_->Close();
        }
        leftBuffer_.clear();
        done_ = true;
    }

} // namespace query

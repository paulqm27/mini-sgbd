#include "page.h"
#include <cstring>

namespace storage {

    Page::Page() {
        data_.resize(PAGE_SIZE, 0);
        InitializeHeader();
    }

    Page::Page(const std::vector<uint8_t>& data) {
        data_.resize(PAGE_SIZE, 0);
        if (data.size() == PAGE_SIZE) {
            data_ = data;
            bool empty = true;
            for (auto byte : data_) {
                if (byte != 0) {
                    empty = false;
                    break;
                }
            }
            if (empty) {
                InitializeHeader();
            }
        }
        else {
            std::memcpy(data_.data(),
                        data.data(),
                        data.size());
            InitializeHeader();
        }
    }

    void Page::InitializeHeader() {
        SetNumSlots(0);
        SetFreeSpacePointer(PAGE_SIZE);
        SetSlotDirectoryEnd(6);
    }

    uint16_t Page::GetNumSlotsInternal() const {
        uint16_t value;
        std::memcpy(&value, data_.data(), 2);
        return value;
    }

    void Page::SetNumSlots(uint16_t n) {
        std::memcpy(data_.data(), &n, 2);
    }

    uint16_t Page::GetFreeSpacePointer() const {
        uint16_t value;
        std::memcpy(&value, data_.data() + 2, 2);
        return value;
    }

    void Page::SetFreeSpacePointer(uint16_t ptr) {
        std::memcpy(data_.data() + 2, &ptr, 2);
    }

    uint16_t Page::GetSlotDirectoryEnd() const {
        uint16_t value;
        std::memcpy(&value, data_.data() + 4, 2);
        return value;
    }

    void Page::SetSlotDirectoryEnd(uint16_t end) {
        std::memcpy(data_.data() + 4, &end, 2);
    }

    Slot Page::ReadSlot(int slotId) const {
        Slot slot;
        int pos = 6 + (slotId * sizeof(Slot));
        std::memcpy(&slot,
                    data_.data() + pos,
                    sizeof(Slot));
        return slot;
    }

    void Page::WriteSlot(int slotId, const Slot& slot) {
        int pos = 6 + (slotId * sizeof(Slot));
        std::memcpy(data_.data() + pos,
                    &slot,
                    sizeof(Slot));
    }

    bool Page::InsertRecord(const std::vector<uint8_t>& record) {
        if (record.empty()) {
            return false;
        }
        uint16_t recordSize = static_cast<uint16_t>(record.size());
        uint16_t freePtr = GetFreeSpacePointer();
        uint16_t slotEnd = GetSlotDirectoryEnd();
        uint16_t requiredSpace = recordSize + sizeof(Slot);
        if (freePtr < slotEnd + requiredSpace) {
            return false;
        }
        freePtr -= recordSize;
        std::memcpy(data_.data() + freePtr, record.data(), recordSize);
        Slot slot;
        slot.offset = freePtr;
        slot.size = recordSize;
        int slotId = GetNumSlotsInternal();
        WriteSlot(slotId, slot);
        SetNumSlots(slotId + 1);
        SetFreeSpacePointer(freePtr);
        SetSlotDirectoryEnd(slotEnd + sizeof(Slot));
        return true;
    }

    std::vector<uint8_t> Page::ReadRecord(int slotId) const {
        std::vector<uint8_t> record;
        if (slotId < 0 ||
            slotId >= GetNumSlotsInternal()) {
            return record;
        }
        Slot slot = ReadSlot(slotId);
        record.resize(slot.size);
        std::memcpy(record.data(),
                    data_.data() + slot.offset,
                    slot.size);
        return record;
    }

    std::vector<std::vector<uint8_t>>
    Page::ReadAllRecords() const {
        std::vector<std::vector<uint8_t>> records;
        int totalSlots = GetNumSlotsInternal();
        for (int i = 0; i < totalSlots; i++) {
            records.push_back(ReadRecord(i));
        }
        return records;
    }

    int Page::GetNumSlots() const {
        return GetNumSlotsInternal();
    }

    const std::vector<uint8_t>&
    Page::GetData() const {
        return data_;
    }
}

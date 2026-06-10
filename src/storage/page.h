#pragma once
#include <vector>
#include <cstdint>
#include <cstddef>

namespace storage {

    constexpr size_t PAGE_SIZE = 4096;

    struct Slot {
        uint16_t offset;
        uint16_t size;
    };

    class Page {

    public:
        Page();
        explicit Page(const std::vector<uint8_t>& data);
        bool InsertRecord(const std::vector<uint8_t>& record);
        std::vector<uint8_t> ReadRecord(int slotId) const;
        std::vector<std::vector<uint8_t>> ReadAllRecords() const;
        int GetNumSlots() const;
        const std::vector<uint8_t>& GetData() const;
        void ReadRaw(size_t offset, void* dest, size_t size) const;
        void WriteRaw(size_t offset, const void* src, size_t size);

    private:
        std::vector<uint8_t> data_;
        void InitializeHeader();
        uint16_t GetNumSlotsInternal() const;
        void SetNumSlots(uint16_t n);
        uint16_t GetFreeSpacePointer() const;
        void SetFreeSpacePointer(uint16_t ptr);
        uint16_t GetSlotDirectoryEnd() const;
        void SetSlotDirectoryEnd(uint16_t end);
        Slot ReadSlot(int slotId) const;
        void WriteSlot(int slotId, const Slot& slot);

    };
}

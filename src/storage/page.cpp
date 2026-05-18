#include "page.h"
#include <cstring>
#include <algorithm>

namespace storage {

Page::Page() {
    data_.resize(PAGE_SIZE, 0);
    InitializeHeader();
}

Page::Page(const std::vector<uint8_t>& data) {
    if (data.size() == PAGE_SIZE) {
        data_ = data;
    } else {
        data_.resize(PAGE_SIZE, 0);
        std::copy(data.begin(), data.end(), data_.begin());
    }
}

void Page::InitializeHeader() {
    SetNumRecords(0);
    SetFreePosition(8); // 4 bytes num records, 4 bytes free position
}

int Page::GetNumRecords() const {
    if (data_.empty()) return 0;
    uint32_t n = 0;
    std::memcpy(&n, data_.data(), 4);
    return static_cast<int>(n);
}

void Page::SetNumRecords(int n) {
    uint32_t un = static_cast<uint32_t>(n);
    std::memcpy(data_.data(), &un, 4);
}

int Page::GetFreePosition() const {
    if (data_.empty()) return 8;
    uint32_t pos = 0;
    std::memcpy(&pos, data_.data() + 4, 4);
    int p = static_cast<int>(pos);
    return p < 8 ? 8 : p;
}

void Page::SetFreePosition(int pos) {
    uint32_t upos = static_cast<uint32_t>(pos);
    std::memcpy(data_.data() + 4, &upos, 4);
}

bool Page::InsertRecord(const std::vector<uint8_t>& record) {
    if (data_.empty()) return false;

    int freePos = GetFreePosition();
    int recordSize = static_cast<int>(record.size());

    if (freePos + recordSize + 4 > static_cast<int>(PAGE_SIZE)) {
        return false;
    }

    // Size of record
    uint32_t size = static_cast<uint32_t>(recordSize);
    std::memcpy(data_.data() + freePos, &size, 4);

    // Record content
    std::memcpy(data_.data() + freePos + 4, record.data(), recordSize);

    SetFreePosition(freePos + 4 + recordSize);
    SetNumRecords(GetNumRecords() + 1);

    return true;
}

std::vector<std::vector<uint8_t>> Page::ReadRecords() const {
    std::vector<std::vector<uint8_t>> records;
    if (data_.empty()) return records;

    int pos = 8;
    int numRecords = GetNumRecords();

    for (int i = 0; i < numRecords; i++) {
        if (pos + 4 > static_cast<int>(PAGE_SIZE)) break;

        uint32_t size = 0;
        std::memcpy(&size, data_.data() + pos, 4);
        int recordSize = static_cast<int>(size);
        pos += 4;

        if (pos + recordSize > static_cast<int>(PAGE_SIZE)) break;

        std::vector<uint8_t> record(recordSize);
        std::memcpy(record.data(), data_.data() + pos, recordSize);
        pos += recordSize;

        records.push_back(std::move(record));
    }

    return records;
}

} // namespace storage

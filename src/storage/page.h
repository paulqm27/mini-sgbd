#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>

namespace storage {

constexpr size_t PAGE_SIZE = 4096;

class Page {
public:
    Page();
    explicit Page(const std::vector<uint8_t>& data);

    int GetNumRecords() const;
    void SetNumRecords(int n);

    int GetFreePosition() const;
    void SetFreePosition(int pos);

    bool InsertRecord(const std::vector<uint8_t>& record);
    std::vector<std::vector<uint8_t>> ReadRecords() const;

    const std::vector<uint8_t>& GetData() const { return data_; }

private:
    std::vector<uint8_t> data_;

    void InitializeHeader();
};

} // namespace storage

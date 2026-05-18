#include "storage/page.h"
#include "storage/storage.h"
#include <iostream>
#include <cassert>
#include <string>

using namespace storage;

void TestPage() {
    std::cout << "Running TestPage..." << std::endl;
    
    Page p;
    assert(p.GetNumRecords() == 0);
    assert(p.GetFreePosition() == 8);

    std::string rec1 = "Juan";
    std::string rec2 = "Maria";
    
    std::vector<uint8_t> data1(rec1.begin(), rec1.end());
    std::vector<uint8_t> data2(rec2.begin(), rec2.end());

    assert(p.InsertRecord(data1) == true);
    assert(p.GetNumRecords() == 1);
    
    assert(p.InsertRecord(data2) == true);
    assert(p.GetNumRecords() == 2);

    auto records = p.ReadRecords();
    assert(records.size() == 2);
    
    std::string out1(records[0].begin(), records[0].end());
    std::string out2(records[1].begin(), records[1].end());

    assert(out1 == "Juan");
    assert(out2 == "Maria");

    std::cout << "TestPage PASSED" << std::endl;
}

void TestStorage() {
    std::cout << "Running TestStorage..." << std::endl;

    std::string filename = "data/test_db.bin";
    
    // Write
    {
        StorageManager sm(filename);
        Page p;
        
        std::string rec = "TestRecord";
        std::vector<uint8_t> data(rec.begin(), rec.end());
        p.InsertRecord(data);
        
        bool success = sm.WritePageData(0, p);
        assert(success == true);
    }
    
    // Read
    {
        StorageManager sm(filename);
        auto p = sm.ReadPageData(0);
        assert(p != nullptr);
        
        auto records = p->ReadRecords();
        assert(records.size() == 1);
        
        std::string out(records[0].begin(), records[0].end());
        assert(out == "TestRecord");
    }

    std::cout << "TestStorage PASSED" << std::endl;
}

int main() {
    std::cout << "Starting Storage Tests (Week 5)...\n" << std::endl;
    
    TestPage();
    TestStorage();
    
    std::cout << "\nAll storage tests passed successfully!" << std::endl;
    return 0;
}

#include "storage/page.h"
#include "storage/storage.h"

#include <iostream>
#include <cassert>
#include <string>

using namespace std;
using namespace storage;

void TestPage() {
    cout << "Ejecutando prueba de paginas..." << endl;

    Page p;

    assert(p.GetNumSlots() == 0);

    string rec1 = "Juan";
    string rec2 = "Maria";

    vector<uint8_t> data1(rec1.begin(), rec1.end());
    vector<uint8_t> data2(rec2.begin(), rec2.end());

    assert(p.InsertRecord(data1) == true);
    assert(p.GetNumSlots() == 1);

    assert(p.InsertRecord(data2) == true);
    assert(p.GetNumSlots() == 2);

    auto records = p.ReadAllRecords();

    assert(records.size() == 2);

    string out1(records[0].begin(), records[0].end());
    string out2(records[1].begin(), records[1].end());

    assert(out1 == "Juan");
    assert(out2 == "Maria");

    auto slot0 = p.ReadRecord(0);
    auto slot1 = p.ReadRecord(1);

    string s0(slot0.begin(), slot0.end());
    string s1(slot1.begin(), slot1.end());

    assert(s0 == "Juan");
    assert(s1 == "Maria");

    cout << "Prueba de paginas completada correctamente" << endl;
}

void TestStorage() {
    cout << "Ejecutando prueba de almacenamiento..." << endl;

    string filename = "../data/test_db.bin";

    {
        StorageManager sm(filename);

        Page p;

        string rec = "TestRecord";

        vector<uint8_t> data(rec.begin(), rec.end());

        p.InsertRecord(data);

        bool success = sm.WritePageData(0, p);

        assert(success == true);
    }

    {
        StorageManager sm(filename);

        auto p = sm.ReadPageData(0);

        assert(p != nullptr);

        auto records = p->ReadAllRecords();

        assert(records.size() == 1);

        string out(records[0].begin(), records[0].end());

        assert(out == "TestRecord");

        auto single = p->ReadRecord(0);

        string singleOut(single.begin(), single.end());

        assert(singleOut == "TestRecord");
    }

    cout << "Prueba de almacenamiento completada correctamente" << endl;
}

int main() {
    cout << "Iniciando pruebas del Storage Manager (Semana 5)...\n" << endl;

    TestPage();
    TestStorage();

    cout << "\nTodas las pruebas finalizaron correctamente." << endl;

    return 0;
}

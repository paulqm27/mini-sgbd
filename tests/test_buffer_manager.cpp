#include "buffer/buffer.h"
#include "storage/storage.h"

#include <cassert>
#include <iostream>
#include <vector>

using namespace std;
using namespace storage;
using namespace buffer;

void TestLRUEviction() {
    cout << "\n--- Ejecutando TestLRUEviction ---" << endl;
    string dbFilename = "test_buffer_lru.db";
    remove(dbFilename.c_str());

    {
        StorageManager sm(dbFilename);
        std::vector<uint8_t> emptyData(PAGE_SIZE, 0);
        for (int i = 0; i < 10; ++i) {
            sm.WritePage(i, emptyData);
        }
    }

    StorageManager sm(dbFilename);
    BufferManager bm(3, &sm);

    cout << "Accediendo a paginas 0, 1, 2 (Pool deberia llenarse)" << endl;
    Frame* f0 = bm.GetPage(0);
    bm.ReleasePage(0, false);
    Frame* f1 = bm.GetPage(1);
    bm.ReleasePage(1, false);
    Frame* f2 = bm.GetPage(2);
    bm.ReleasePage(2, false);

    bm.PrintStatus();

    cout << "Accediendo a pagina 3 (Deberia desalojar pagina 0, que es la LRU)" << endl;
    Frame* f3 = bm.GetPage(3);
    bm.ReleasePage(3, false);

    bm.PrintStatus();

    bm.ResetStats();
    bm.GetPage(1); bm.ReleasePage(1, false);
    bm.GetPage(2); bm.ReleasePage(2, false);
    bm.GetPage(3); bm.ReleasePage(3, false);
    assert(bm.GetHitCount() == 3);

    bm.ResetStats();
    bm.GetPage(0); bm.ReleasePage(0, false);
    assert(bm.GetMissCount() == 1);

    cout << "TestLRUEviction PASSED!" << endl;
}

void TestPinning() {
    cout << "\n--- Ejecutando TestPinning ---" << endl;
    string dbFilename = "test_buffer_pin.db";
    remove(dbFilename.c_str());

    {
        StorageManager sm(dbFilename);
        std::vector<uint8_t> emptyData(PAGE_SIZE, 0);
        for (int i = 0; i < 10; ++i) {
            sm.WritePage(i, emptyData);
        }
    }

    StorageManager sm(dbFilename);
    BufferManager bm(3, &sm);

    cout << "Cargando paginas 0 (pinned), 1 (unpinned), 2 (unpinned)..." << endl;
    Frame* f0 = bm.GetPage(0);

    // Cargar y liberar de inmediato 1 y 2
    Frame* f1 = bm.GetPage(1); bm.ReleasePage(1, false);
    Frame* f2 = bm.GetPage(2); bm.ReleasePage(2, false);

    bm.PrintStatus();
    assert(f0->pinCount == 1);

    cout << "Accediendo a paginas 3 y 4 (Forzando reemplazo. Pagina 0 no debe desalojarse por estar pinned)" << endl;
    Frame* f3 = bm.GetPage(3); bm.ReleasePage(3, false);
    Frame* f4 = bm.GetPage(4); bm.ReleasePage(4, false);

    bm.PrintStatus();

    bm.ResetStats();
    bm.GetPage(0); bm.ReleasePage(0, false);
    bm.GetPage(3); bm.ReleasePage(3, false);
    bm.GetPage(4); bm.ReleasePage(4, false);
    assert(bm.GetHitCount() == 3);

    bm.ReleasePage(0, false);
    assert(f0->pinCount == 0);

    cout << "Accediendo a pagina 5 (Ahora pagina 0 deberia ser desalojada)" << endl;
    Frame* f5 = bm.GetPage(5); bm.ReleasePage(5, false);

    bm.PrintStatus();

    bm.ResetStats();
    bm.GetPage(0); bm.ReleasePage(0, false);
    assert(bm.GetMissCount() == 1);

    cout << "TestPinning PASSED!" << endl;
}

void TestDirtyWriteback() {
    cout << "\n--- Ejecutando TestDirtyWriteback ---" << endl;
    string dbFilename = "test_buffer_dirty.db";
    remove(dbFilename.c_str());

    {
        StorageManager sm(dbFilename);
        std::vector<uint8_t> emptyData(PAGE_SIZE, 0);
        sm.WritePage(0, emptyData);
        sm.WritePage(1, emptyData);
        sm.WritePage(2, emptyData);
        sm.WritePage(3, emptyData);
    }

    {
        StorageManager sm(dbFilename);
        BufferManager bm(3, &sm);

        Frame* f0 = bm.GetPage(0);
        string testMsg = "Datos Sucios En Pagina 0";
        vector<uint8_t> rec(testMsg.begin(), testMsg.end());
        f0->page->InsertRecord(rec);
        bm.ReleasePage(0, true);

        Frame* f1 = bm.GetPage(1); bm.ReleasePage(1, false);
        Frame* f2 = bm.GetPage(2); bm.ReleasePage(2, false);

        cout << "Forzando desalojo de pagina 0 (que esta dirty)..." << endl;
        Frame* f3 = bm.GetPage(3); bm.ReleasePage(3, false);
    }

    StorageManager smReload(dbFilename);
    auto p0 = smReload.ReadPageData(0);
    auto records = p0->ReadAllRecords();
    assert(records.size() == 1);
    string recText(records[0].begin(), records[0].end());
    assert(recText == "Datos Sucios En Pagina 0");

    cout << "TestDirtyWriteback PASSED!" << endl;
}

int main() {
    cout << "=========================================================" << endl;
    cout << "          PRUEBA DEL BUFFER MANAGER (LRU)                " << endl;
    cout << "=========================================================" << endl;

    TestLRUEviction();
    TestPinning();
    TestDirtyWriteback();

    cout << "\nTodas las pruebas del Buffer Manager completadas con exito!" << endl;
    return 0;
}

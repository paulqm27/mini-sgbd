#include "index/bplus_tree.h"
#include "buffer/buffer.h"
#include "storage/storage.h"

#include <cassert>
#include <iostream>
#include <vector>
#include <algorithm>

using namespace std;
using namespace index_m;
using namespace storage;
using namespace buffer;

void TestSimpleInsertion() {
    cout << "\n--- Ejecutando TestSimpleInsertion ---" << endl;
    string dbFilename = "test_bplus_insert_simple.db";
    remove(dbFilename.c_str());

    StorageManager sm(dbFilename);
    BufferManager bm(10, &sm);
    BPlusTree tree(&bm, &sm, 3, 3);

    // Insertar claves de forma determinista
    vector<int> keys = {10, 20, 5, 15, 30, 25, 35};
    cout << "Insertando claves: ";
    for (int k : keys) {
        cout << k << " ";
        tree.Insert(k, RID{1, k});
    }
    cout << "\nEstructura final del arbol:" << endl;
    tree.PrintTree();

    // Validar búsquedas
    for (int k : keys) {
        RID r = tree.Search(k);
        assert(r.IsValid());
        assert(r.pageId == 1 && r.slotId == k);
    }

    // Validar clave inexistente
    RID r = tree.Search(999);
    assert(!r.IsValid());
    cout << "TestSimpleInsertion PASSED!" << endl;
}

void TestDuplicateInsertion() {
    cout << "\n--- Ejecutando TestDuplicateInsertion ---" << endl;
    string dbFilename = "test_bplus_insert_dup.db";
    remove(dbFilename.c_str());

    StorageManager sm(dbFilename);
    BufferManager bm(10, &sm);
    BPlusTree tree(&bm, &sm, 3, 3);

    // Insertar clave duplicada
    tree.Insert(10, RID{1, 10});
    RID r1 = tree.Search(10);
    assert(r1.pageId == 1 && r1.slotId == 10);

    // Sobrescribir con nuevo RID
    tree.Insert(10, RID{2, 20});
    RID r2 = tree.Search(10);
    assert(r2.pageId == 2 && r2.slotId == 20);

    cout << "TestDuplicateInsertion PASSED!" << endl;
}

void TestMassiveInsertion() {
    cout << "\n--- Ejecutando TestMassiveInsertion ---" << endl;
    string dbFilename = "test_bplus_insert_massive.db";
    remove(dbFilename.c_str());

    StorageManager sm(dbFilename);
    BufferManager bm(20, &sm);
    BPlusTree tree(&bm, &sm, 3, 3);

    // Generar 100 claves y barajarlas
    vector<int> keys;
    for (int i = 1; i <= 100; ++i) {
        keys.push_back(i);
    }
    // Barajamos de forma determinista
    for (size_t i = 0; i < keys.size(); ++i) {
        size_t j = (i * 31 + 7) % keys.size();
        swap(keys[i], keys[j]);
    }

    cout << "Insertando 100 claves..." << endl;
    for (int k : keys) {
        tree.Insert(k, RID{1, k});
    }

    cout << "Validando 100 claves insertadas..." << endl;
    for (int k : keys) {
        RID r = tree.Search(k);
        if (!r.IsValid()) {
            cout << "ERROR: Clave [" << k << "] NO ENCONTRADA!" << endl;
            assert(false);
        }
    }

    cout << "Estructura final del arbol masivo:" << endl;
    tree.PrintTree();
    cout << "TestMassiveInsertion PASSED!" << endl;
}

int main() {
    cout << "=========================================================" << endl;
    cout << "          PRUEBA DE INSERCION EN ARBOL B+                " << endl;
    cout << "=========================================================" << endl;

    TestSimpleInsertion();
    TestDuplicateInsertion();
    TestMassiveInsertion();

    cout << "\nTodas las pruebas de insercion completadas con exito!" << endl;
    return 0;
}

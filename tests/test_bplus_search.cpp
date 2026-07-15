#include "index/bplus_tree.h"
#include "buffer/buffer.h"
#include "storage/storage.h"

#include <cassert>
#include <iostream>
#include <vector>

using namespace std;
using namespace index_m;
using namespace storage;
using namespace buffer;

void TestBPlusSearch() {
    cout << "=========================================================" << endl;
    cout << "          PRUEBA DE BUSQUEDA EN ARBOL B+                 " << endl;
    cout << "=========================================================" << endl;

    string db = "test_bplus_search.db";
    remove(db.c_str());

    StorageManager sm(db);
    BufferManager bm(10, &sm);
    BPlusTree tree(&bm, &sm, 3, 3);

    // Insertar un conjunto conocido de claves y RIDs
    vector<pair<int, RID>> data = {
        {10, RID{100, 1}},
        {20, RID{200, 2}},
        {30, RID{300, 3}},
        {40, RID{400, 4}},
        {50, RID{500, 5}}
    };

    cout << "Insertando claves de prueba..." << endl;
    for (const auto& item : data) {
        tree.Insert(item.first, item.second);
    }

    // Probar búsquedas de claves que existen
    cout << "\nProbando busquedas de claves existentes:" << endl;
    for (const auto& item : data) {
        int key = item.first;
        RID expectedRID = item.second;
        
        cout << "  Buscando clave [" << key << "]... ";
        RID foundRID = tree.Search(key);
        
        if (foundRID.IsValid()) {
            cout << "FOUND! RID = (Pagina: " << foundRID.pageId << ", Slot: " << foundRID.slotId << ")" << endl;
            assert(foundRID.pageId == expectedRID.pageId);
            assert(foundRID.slotId == expectedRID.slotId);
        } else {
            cout << "NOT FOUND! (Error: deberia existir)" << endl;
            assert(false);
        }
    }

    // Probar búsquedas de claves inexistentes
    vector<int> missingKeys = {0, 15, 45, 999};
    cout << "\nProbando busquedas de claves inexistentes:" << endl;
    for (int key : missingKeys) {
        cout << "  Buscando clave [" << key << "]... ";
        RID foundRID = tree.Search(key);
        
        if (foundRID.IsValid()) {
            cout << "FOUND! (Error: no deberia existir, RID = " << foundRID.pageId << "," << foundRID.slotId << ")" << endl;
            assert(false);
        } else {
            cout << "NOT FOUND!" << endl;
        }
    }

    cout << "\nTodas las pruebas de busqueda completadas con exito!" << endl;
}

int main() {
    TestBPlusSearch();
    return 0;
}

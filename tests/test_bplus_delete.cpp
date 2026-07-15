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

static void AssertSearch(const BPlusTree& tree, int key, bool shouldExist) {
    RID found = tree.Search(key);
    if (shouldExist) {
        assert(found.IsValid());
    } else {
        assert(!found.IsValid());
    }
}

void TestBPlusDeleteMerge() {
    cout << "\n--- Ejecutando TestBPlusDeleteMerge ---" << endl;

    string db = "test_bplus_delete_merge.db";
    remove(db.c_str());

    StorageManager sm(db);
    BufferManager bm(10, &sm);
    BPlusTree tree(&bm, &sm, 3, 3);

    // Insertar claves 1..20
    for (int k = 1; k <= 20; ++k) {
        RID r{1, k};
        tree.Insert(k, r);
    }

    // Verificar algunas claves
    for (int k : {5, 10, 15, 20}) {
        RID found = tree.Search(k);
        assert(found.IsValid());
    }

    cout << "Arbol inicial:" << endl;
    tree.PrintTree();

    // Eliminaciones en bloques (merge / rebalanceo)
    vector<int> firstBatch = {2, 3, 4, 5, 6, 7, 8, 9};
    cout << "Eliminando primer bloque (2..9)..." << endl;
    for (int k : firstBatch) {
        assert(tree.Delete(k));
    }

    vector<int> secondBatch = {11, 12, 13, 14};
    cout << "Eliminando segundo bloque (11..14)..." << endl;
    for (int k : secondBatch) {
        assert(tree.Delete(k));
    }

    cout << "Arbol intermedio:" << endl;
    tree.PrintTree();

    vector<int> thirdBatch = {17, 18, 19};
    cout << "Eliminando tercer bloque (17..19)..." << endl;
    for (int k : thirdBatch) {
        assert(tree.Delete(k));
    }

    // Verificar eliminados
    for (int k : {2,3,4,5,6,7,8,9,11,12,13,14,17,18,19}) {
        RID f = tree.Search(k);
        assert(!f.IsValid());
    }

    // Verificar restantes
    for (int k : {1, 10, 15, 16, 20}) {
        RID f = tree.Search(k);
        assert(f.IsValid());
    }

    cout << "Eliminando claves 1 y 15..." << endl;
    for (int k : {1, 15}) {
        assert(tree.Delete(k));
    }

    for (int k : {1, 15}) {
        assert(!tree.Search(k).IsValid());
    }

    for (int k : {10, 16, 20}) {
        assert(tree.Search(k).IsValid());
    }

    cout << "Arbol final:" << endl;
    tree.PrintTree();

    cout << "TestBPlusDeleteMerge PASSED!" << endl;
}

int main() {
    cout << "=========================================================" << endl;
    cout << "        PRUEBA DE ELIMINACION EN ARBOL B+                " << endl;
    cout << "=========================================================" << endl;

    TestBPlusDeleteMerge();

    cout << "\nTodas las pruebas de eliminacion completadas con exito!" << endl;
    return 0;
}

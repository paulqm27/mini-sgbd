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
    cout << "Ejecutando prueba Delete/Merge del B+ Tree..." << endl;

    string db = "data/test_bplus_delete.db";
    remove(db.c_str());

    StorageManager sm(db);
    BufferManager bm(10, &sm);
    BPlusTree tree(&bm, &sm, 3, 3);

    // Insertar claves 1..20
    vector<RID> rids;
    for (int k = 1; k <= 20; ++k) {
        RID r{1, k};
        tree.Insert(k, r);
        rids.push_back(r);
    }

    // Verificar que algunas claves existen
    for (int k : {5, 10, 15, 20}) {
        RID found = tree.Search(k);
        assert(found.IsValid());
    }

    auto DumpNode = [&](int pageId) {
        cout << "  Dump page " << pageId << ":\n";
        auto frame = bm.GetPage(pageId);
        BPlusTreeNode node(frame->page.get());
        cout << "    isLeaf=" << node.IsLeaf() << " numKeys=" << node.GetNumKeys()
             << " parent=" << node.GetParentPageId();
        if (node.IsLeaf()) {
            cout << " next=" << node.GetNextPageId() << " prev=" << node.GetPrevPageId();
        }
        cout << "\n";
        for (int i = 0; i < node.GetNumKeys(); ++i) {
            cout << "    key[" << i << "]=" << node.GetKey(i);
            if (node.IsLeaf()) {
                RID r = node.GetRID(i, 3);
                cout << " rid=" << r.pageId << "," << r.slotId;
            } else {
                cout << " child=" << node.GetChild(i, 3);
            }
            cout << "\n";
        }
        if (!node.IsLeaf()) {
            cout << "    child[" << node.GetNumKeys() << "]=" << node.GetChild(node.GetNumKeys(), 3) << "\n";
        }
        bm.ReleasePage(pageId, false);
    };

    cout << "  Dump inicial de page 19:" << endl;
    DumpNode(19);

    // Eliminar varias claves para forzar merges y reequilibrio
    for (int k : {2,3,4,5,6,7,8,9}) {
        cout << "  - borrando " << k << "\n" << flush;
        bool ok = tree.Delete(k);
        cout << "    -> resultado: " << (ok ? "OK" : "FAIL") << "\n" << flush;
        if (!ok) {
            cerr << "Fallo eliminando " << k << "\n";
            return;
        }
    }

    for (int k : {11,12,13,14}) {
        cout << "  - borrando " << k << endl;
        RID prev = tree.Search(k);
        cout << "    antes: " << (prev.IsValid() ? "encontrada" : "no encontrada") << endl;
        bool ok = tree.Delete(k);
        cout << "    delete: " << (ok ? "OK" : "FAIL") << endl;
        if (!ok) {
            cerr << "Fallo en delete(" << k << ")" << endl;
            return;
        }
        if (k == 11) {
            cout << "  - Estructura tras borrar 11:" << endl;
            tree.PrintTree();
            DumpNode(3);
            DumpNode(19);
            DumpNode(12);
        }
    }

    for (int k : {17,18,19}) {
        cout << "  - borrando " << k << endl;
        bool ok = tree.Delete(k);
        assert(ok);
    }

    // Verificar que las claves eliminadas no existen
    for (int k : {2,3,4,5,6,7,8,9,11,12,13,14,17,18,19}) {
        RID f = tree.Search(k);
        assert(!f.IsValid());
    }

    // Verificar que las claves restantes siguen accesibles
    for (int k : {1,10,15,16,20}) {
        RID f = tree.Search(k);
        assert(f.IsValid());
    }

    // Eliminar algunas claves adicionales para forzar merge en el nivel superior
    for (int k : {1,15}) {
        bool ok = tree.Delete(k);
        assert(ok);
    }

    for (int k : {1,15}) {
        RID f = tree.Search(k);
        assert(!f.IsValid());
    }

    // Verificar que las claves finales siguen accesibles
    for (int k : {10,16,20}) {
        RID f = tree.Search(k);
        assert(f.IsValid());
    }

    cout << "Prueba Delete/Merge completada correctamente" << endl;
}

void TestBPlusRootCollapse() {
    cout << "Ejecutando prueba Root Collapse del B+ Tree..." << endl;

    string db = "data/test_bplus_root_collapse.db";
    remove(db.c_str());

    StorageManager sm(db);
    BufferManager bm(10, &sm);
    BPlusTree tree(&bm, &sm, 3, 3);

    for (int k = 1; k <= 7; ++k) {
        tree.Insert(k, RID{1, k});
    }

    for (int k : {1, 2, 3, 4, 5}) {
        bool ok = tree.Delete(k);
        assert(ok);
    }

    for (int k : {1, 2, 3, 4, 5}) {
        AssertSearch(tree, k, false);
    }
    for (int k : {6, 7}) {
        AssertSearch(tree, k, true);
    }

    int rootId = tree.GetRootPageId();
    auto frame = bm.GetPage(rootId);
    BPlusTreeNode rootNode(frame->page.get());
    assert(rootNode.IsLeaf());
    bm.ReleasePage(rootId, false);

    cout << "Prueba Root Collapse completada correctamente" << endl;
}

int main() {
    TestBPlusDeleteMerge();
    TestBPlusRootCollapse();
    cout << "OK" << endl;
    return 0;
}

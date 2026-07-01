#include "index/bplus_tree.h"
#include "buffer/buffer.h"
#include "storage/storage.h"

#include <iostream>
#include <string>
#include <vector>

using namespace std;
using namespace index_m;
using namespace storage;
using namespace buffer;

static void PrintSection(const string& title) {
    cout << "\n============================================================" << endl;
    cout << title << endl;
    cout << "============================================================" << endl;
}

int main() {
    PrintSection("DEMOSTRACION DEL B+ TREE CON BUFFER MANAGER Y PERSISTENCIA");

    string dbFilename = "data/bplus_delete_demo.db";
    remove(dbFilename.c_str());

    cout << "[A] Inicializando componentes..." << endl;
    StorageManager storageManager(dbFilename);
    BufferManager bufferManager(10, &storageManager);
    BPlusTree tree(&bufferManager, &storageManager, 3, 3);

    cout << "[B] Insertando claves de demostracion..." << endl;
    for (int key = 1; key <= 20; ++key) {
        tree.Insert(key, RID{1, key});
    }
    cout << "  - Claves insertadas: 1..20" << endl;
    cout << "  - Estructura inicial del arbol:" << endl;
    tree.PrintTree();

    cout << "[C] Eliminando claves para probar merge y reequilibrio..." << endl;
    vector<int> keysToDelete = {1, 3, 7, 15, 20};
    for (int key : keysToDelete) {
        bool deleted = tree.Delete(key);
        cout << "  - Eliminando clave [" << key << "]... " << (deleted ? "OK" : "FAIL") << endl;
    }

    cout << "  - Estructura despues de las eliminaciones:" << endl;
    tree.PrintTree();

    cout << "[D] Verificando busquedas tras la eliminacion..." << endl;
    for (int key : keysToDelete) {
        RID found = tree.Search(key);
        cout << "  - Clave [" << key << "] => " << (found.IsValid() ? "AUN EXISTE" : "ELIMINADA") << endl;
    }
    for (int key : {2, 8, 16, 18}) {
        RID found = tree.Search(key);
        cout << "  - Clave [" << key << "] => " << (found.IsValid() ? "AUN EXISTE" : "ELIMINADA") << endl;
    }

    cout << "[E] Persistiendo cambios y recargando desde disco..." << endl;
    bufferManager.Flush();
    {
        StorageManager reloadedStorage(dbFilename);
        BufferManager reloadedBuffer(10, &reloadedStorage);
        BPlusTree reloadedTree(&reloadedBuffer, &reloadedStorage, 3, 3);

        cout << "  - Root page id recargado: " << reloadedTree.GetRootPageId() << endl;
        reloadedTree.PrintTree();

        cout << "  - Verificacion tras recarga:" << endl;
        for (int key : {2, 8, 16, 18}) {
            RID found = reloadedTree.Search(key);
            cout << "    * Clave [" << key << "] => " << (found.IsValid() ? "AUN EXISTE" : "ELIMINADA") << endl;
        }
        reloadedBuffer.Flush();
    }

    cout << "\nDemo completado exitosamente." << endl;
    return 0;
}

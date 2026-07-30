#include "index/bplus_tree.h"
#include "buffer/buffer.h"
#include "storage/storage.h"

#include <cassert>
#include <iostream>
#include <vector>
#include <string>

using namespace std;
using namespace index_m;
using namespace storage;
using namespace buffer;

struct TableRID {
    int32_t pageId;
    int32_t slotId;
};

TableRID InsertRecordToTable(BufferManager& bm, const string& recordStr, int& currentDataPageId) {
    vector<uint8_t> record(recordStr.begin(), recordStr.end());
    Frame* frame = bm.GetPage(currentDataPageId);

    if (!frame->page->InsertRecord(record)) {
        bm.ReleasePage(currentDataPageId, false);
        currentDataPageId++;
        frame = bm.GetPage(currentDataPageId);
        frame->page->InsertRecord(record);
    }

    TableRID rid;
    rid.pageId = currentDataPageId;
    rid.slotId = frame->page->GetNumSlots() - 1;

    bm.ReleasePage(currentDataPageId, true);
    return rid;
}

void TestPersistence() {
    cout << "=========================================================" << endl;
    cout << "          PRUEBA DE PERSISTENCIA (SGBD)                  " << endl;
    cout << "=========================================================" << endl;

    string dbFilename = "test_persistence_sgbd.db";
    remove(dbFilename.c_str());

    int currentDataPageId = 1;
    int maxKeys = 3;

    {
        cout << "[Fase 1] Creando componentes..." << endl;
        StorageManager sm(dbFilename);
        BufferManager bm(10, &sm);
        BPlusTree tree(&bm, &sm, maxKeys, maxKeys);

        vector<pair<int, string>> studentRecords = {
            {10, "Alumno 10: Paul, Nota: 18"},
            {20, "Alumno 20: Maria, Nota: 15"},
            {30, "Alumno 30: Juan, Nota: 16"},
            {40, "Alumno 40: Lucia, Nota: 20"}
        };

        cout << "  Escribiendo registros de datos en disco..." << endl;
        vector<pair<int, RID>> indexedRecords;
        for (const auto& record : studentRecords) {
            int key = record.first;
            string text = record.second;

            TableRID trid = InsertRecordToTable(bm, text, currentDataPageId);
            indexedRecords.push_back({key, RID{trid.pageId, trid.slotId}});
        }

        bm.Flush();
        cout << "  Paginas de datos sincronizadas. Paginas en disco: " << sm.GetNumPages() << endl;

        cout << "  Construyendo el indice B+ Tree..." << endl;
        for (const auto& item : indexedRecords) {
            tree.Insert(item.first, item.second);
            cout << "    Insertado en arbol: Clave [" << item.first << "] -> RID(" << item.second.pageId << "," << item.second.slotId << ")" << endl;
        }

        cout << "Estructura del arbol antes del cierre:" << endl;
        tree.PrintTree();
        bm.Flush();
        cout << "[Fase 1] Cambios persistidos. Cerrando componentes." << endl;
    }

    {
        cout << "\n[Fase 2] Reabriendo base de datos para verificar persistencia..." << endl;
        StorageManager sm(dbFilename);
        BufferManager bm(10, &sm);
        BPlusTree tree(&bm, &sm, maxKeys, maxKeys);

        int rootId = tree.GetRootPageId();
        cout << "  ID de pagina raiz recargado: " << rootId << endl;
        assert(rootId >= 0);

        cout << "Estructura del arbol recargado:" << endl;
        tree.PrintTree();

        vector<pair<int, string>> expectedRecords = {
            {10, "Alumno 10: Paul, Nota: 18"},
            {20, "Alumno 20: Maria, Nota: 15"},
            {30, "Alumno 30: Juan, Nota: 16"},
            {40, "Alumno 40: Lucia, Nota: 20"}
        };

        for (const auto& item : expectedRecords) {
            int key = item.first;
            string expectedText = item.second;

            cout << "  Buscando clave [" << key << "]... ";
            RID foundRID = tree.Search(key);

            if (foundRID.IsValid()) {
                cout << "FOUND! RID = (" << foundRID.pageId << "," << foundRID.slotId << ")" << endl;

                Frame* frame = bm.GetPage(foundRID.pageId);
                vector<uint8_t> bytes = frame->page->ReadRecord(foundRID.slotId);
                bm.ReleasePage(foundRID.pageId, false);

                string actualText(bytes.begin(), bytes.end());
                cout << "    Contenido: \"" << actualText << "\"" << endl;
                assert(actualText == expectedText);
            } else {
                cout << "NOT FOUND! (Error: deberia persistir)" << endl;
                assert(false);
            }
        }
    }

    cout << "\nPruebas de persistencia completadas con exito!" << endl;
}

int main() {
    TestPersistence();
    return 0;
}

#include <iostream>
#include <string>
#include <vector>

#include "../storage/storage.h"
#include "../buffer/buffer.h"
#include "scan.h"
#include "select.h"
#include "project.h"

using namespace std;
using namespace storage;
using namespace buffer;
using namespace exec;

// Inserta registros de texto en páginas usando BufferManager
void InsertRecords(BufferManager& bm, int& currentPageId, const vector<string>& records) {
    for (const auto& s : records) {
        vector<uint8_t> data(s.begin(), s.end());
        Frame* frame = bm.GetPage(currentPageId);
        if (!frame->page->InsertRecord(data)) {
            bm.ReleasePage(currentPageId, false);
            currentPageId++;
            frame = bm.GetPage(currentPageId);
            frame->page->InsertRecord(data);
        }
        bm.ReleasePage(currentPageId, true);
    }
}

int main() {
    string dbFilename = "data/demo_iter.db";
    remove(dbFilename.c_str());

    StorageManager storageManager(dbFilename);
    BufferManager bufferManager(10, &storageManager);

    int currentPageId = 1;
    vector<string> records;
    for (int i = 1; i <= 20; ++i) {
        records.push_back("Registro #" + to_string(i));
    }

    InsertRecords(bufferManager, currentPageId, records);
    bufferManager.Flush();

    cout << "Datos insertados hasta la pagina: " << currentPageId << endl;

    // Scan sobre todas las paginas usadas
    Scan scan(&bufferManager, 1, currentPageId);

    // Select: filtrar registros que contengan "5"
    auto pred = [](const vector<uint8_t>& bytes) {
        string s(bytes.begin(), bytes.end());
        return s.find('5') != string::npos;
    };

    // Project: dejar solo los primeros 12 caracteres
    auto proj = [](const vector<uint8_t>& bytes) {
        string s(bytes.begin(), bytes.end());
        if (s.size() > 12) s = s.substr(0, 12);
        return vector<uint8_t>(s.begin(), s.end());
    };

    Select sel(&scan, pred);
    Project projOp(&sel, proj);

    projOp.open();
    vector<uint8_t> out;
    cout << "Resultados (Scan -> Select(contiene '5') -> Project(substr 0..12)):\n";
    while (projOp.next(out)) {
        string s(out.begin(), out.end());
        cout << " - " << s << "\n";
    }
    projOp.close();

    cout << "Demo finalizada." << endl;
    return 0;
}

#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <cstdio>

#include "../storage/storage.h"
#include "../buffer/buffer.h"
#include "../index/bplus_tree.h"
#include "scan.h"
#include "select.h"
#include "project.h"
#include "nested_loop_join.h"
#include "index_scan.h"
#include "planner.h"

using namespace std;
using namespace storage;
using namespace buffer;
using namespace index_m;
using namespace exec;

struct TableRID { int pageId; int slotId; };

void InsertRecord(BufferManager& bm, int& pageId, const string& payload) {
    vector<uint8_t> data(payload.begin(), payload.end());
    Frame* frame = bm.GetPage(pageId);
    if (!frame->page->InsertRecord(data)) {
        bm.ReleasePage(pageId, false);
        pageId++;
        frame = bm.GetPage(pageId);
        frame->page->InsertRecord(data);
    }
    bm.ReleasePage(pageId, true);
}

vector<uint8_t> MakeRecord(int key, const string& label) {
    string s = label + " Key:" + to_string(key);
    return vector<uint8_t>(s.begin(), s.end());
}

int ParseKey(const vector<uint8_t>& rec) {
    string s(rec.begin(), rec.end());
    auto pos = s.find("Key:");
    if (pos == string::npos) return -1;
    try { return stoi(s.substr(pos + 4)); } catch(...) { return -1; }
}

int main() {
    const string dbFilename = "data/full_flow.db";
    remove(dbFilename.c_str());

    StorageManager storageManager(dbFilename);
    BufferManager bufferManager(20, &storageManager);

    cout << "=== DEMO UNIFICADO SEMANA 14-15-16 ===\n";
    cout << "1. Insertar datos con BufferManager...\n";

    int leftPage = 1;
    int rightPage = 20;
    vector<int> leftKeys{1,2,3,4,5,6,7,8,9,10};
    vector<int> rightKeys{5,6,7,8,9,10,11,12,13,14};

    for (int k : leftKeys) {
        InsertRecord(bufferManager, leftPage, "Left Key:" + to_string(k));
    }
    for (int k : rightKeys) {
        InsertRecord(bufferManager, rightPage, "Right Key:" + to_string(k));
    }
    bufferManager.Flush();

    cout << "  - Datos insertados. Left hasta pagina " << leftPage << ", Right hasta pagina " << rightPage << "\n";

    cout << "\n2. Ejecutar Scan -> Select -> Project...\n";
    Scan scan(&bufferManager, 1, leftPage);
    auto pred = [](const vector<uint8_t>& rec) {
        string s(rec.begin(), rec.end());
        return s.find("Key:1") != string::npos || s.find("Key:2") != string::npos;
    };
    auto proj = [](const vector<uint8_t>& rec) {
        string s(rec.begin(), rec.end());
        return vector<uint8_t>(s.begin(), s.begin() + min<size_t>(s.size(), 11));
    };
    Select sel(&scan, pred);
    Project project(&sel, proj);

    project.open();
    vector<uint8_t> out;
    cout << "  - Resultados:\n";
    while (project.next(out)) {
        cout << "    * " << string(out.begin(), out.end()) << "\n";
    }
    project.close();

    cout << "\n3. Construir B+ Tree para IndexScan...\n";
    BPlusTree bpt(&bufferManager, &storageManager, 3, 3);
    for (int pid = 20; pid <= rightPage; ++pid) {
        Frame* frame = bufferManager.GetPage(pid);
        int numSlots = frame->page->GetNumSlots();
        for (int slot = 0; slot < numSlots; ++slot) {
            auto rec = frame->page->ReadRecord(slot);
            int key = ParseKey(rec);
            if (key >= 0) {
                index_m::RID rid{pid, slot};
                bpt.Insert(key, rid);
            }
        }
        bufferManager.ReleasePage(pid, false);
    }
    bufferManager.Flush();
    cout << "  - B+ Tree construido. Root page = " << bpt.GetRootPageId() << "\n";

    cout << "\n4. Usar heuristica index-aware con IndexScan para buscar una clave exacta...\n";
    PredicateInfo info;
    info.is_equality = true;
    info.key = 8;
    if (Planner::UseIndexScan(&bpt, info)) {
        IndexScan idxScan(&bpt, &bufferManager, info.key);
        idxScan.open();
        if (idxScan.next(out)) {
            cout << "  - IndexScan encontro: " << string(out.begin(), out.end()) << "\n";
        } else {
            cout << "  - IndexScan no encontro registros.\n";
        }
        idxScan.close();
    } else {
        cout << "  - Se decidio no usar IndexScan.\n";
    }

    cout << "\n5. Ejecutar Nested Loop Join Left x Right (igualdad de clave)...\n";
    Scan leftScan(&bufferManager, 1, leftPage);
    Scan rightScan(&bufferManager, 20, rightPage);
    auto joinPred = [](const vector<uint8_t>& l, const vector<uint8_t>& r) {
        return ParseKey(l) == ParseKey(r);
    };
    NestedLoopJoin nlj(&leftScan, &rightScan, joinPred);

    nlj.open();
    int joinCount = 0;
    while (nlj.next(out)) {
        cout << "    * " << string(out.begin(), out.end()) << "\n";
        joinCount++;
    }
    nlj.close();
    cout << "  - Join encontrado " << joinCount << " parejas.\n";

    cout << "\n6. Benchmark de BufferManager para medir hit rate...\n";
    bufferManager.ResetStats();
    auto t0 = chrono::high_resolution_clock::now();
    for (int iter = 0; iter < 3; ++iter) {
        for (int pid = 1; pid <= rightPage; ++pid) {
            Frame* f = bufferManager.GetPage(pid);
            if (f && f->page) {
                volatile int n = f->page->GetNumSlots(); (void)n;
            }
            bufferManager.ReleasePage(pid, false);
        }
    }
    auto t1 = chrono::high_resolution_clock::now();
    double elapsed = chrono::duration<double, milli>(t1 - t0).count();
    cout << "  - Accesos: " << bufferManager.GetAccessCount() << ", Hits: " << bufferManager.GetHitCount() << ", Misses: " << bufferManager.GetMissCount() << ", Hit rate: " << bufferManager.GetHitRate() << ", Tiempo: " << elapsed << " ms\n";

    cout << "\n=== FIN DEMO UNIFICADO ===\n";
    return 0;
}

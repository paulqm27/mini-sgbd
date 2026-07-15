#include <iostream>
#include <vector>
#include <string>

#include "../storage/storage.h"
#include "../buffer/buffer.h"
#include "../index/bplus_tree.h"
#include "scan.h"
#include "nested_loop_join.h"
#include "index_scan.h"

using namespace std;
using namespace storage;
using namespace buffer;
using namespace index_m;
using namespace exec;

// Inserta registros en la misma forma "Key:<k> Val:..." y construye un indice sobre Key
void InsertKeyedRecords(BufferManager& bm, int& currentPageId, const vector<int>& keys, const string& prefix) {
    for (int k : keys) {
        string s = prefix + " Key:" + to_string(k) + "";
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

int ParseKey(const vector<uint8_t>& rec) {
    string s(rec.begin(), rec.end());
    auto pos = s.find("Key:");
    if (pos == string::npos) return -1;
    try { return stoi(s.substr(pos + 4)); } catch(...) { return -1; }
}

int main() {
    string dbFilename = "data/demo_join.db";
    remove(dbFilename.c_str());

    StorageManager storageManager(dbFilename);
    BufferManager bufferManager(20, &storageManager);

    int currentPageId = 1;
    vector<int> leftKeys; for (int i=1;i<=10;++i) leftKeys.push_back(i);
    vector<int> rightKeys; for (int i=5;i<=15;++i) rightKeys.push_back(i);

    InsertKeyedRecords(bufferManager, currentPageId, leftKeys, "L");
    int leftLastPage = currentPageId;
    currentPageId = 20; // separar espacio para la otra tabla
    InsertKeyedRecords(bufferManager, currentPageId, rightKeys, "R");
    int rightLastPage = currentPageId;

    bufferManager.Flush();

    cout << "Left pages end: " << leftLastPage << ", Right pages end: " << rightLastPage << endl;

    // Nested Loop Join demo (materializa inner = right)
    Scan leftScan(&bufferManager, 1, leftLastPage);
    Scan rightScan(&bufferManager, 20, rightLastPage);

    auto joinPred = [](const vector<uint8_t>& l, const vector<uint8_t>& r) {
        int lk = ParseKey(l);
        int rk = ParseKey(r);
        return lk != -1 && rk != -1 && lk == rk;
    };

    NestedLoopJoin nlj(&leftScan, &rightScan, joinPred);
    nlj.open();
    vector<uint8_t> out;
    cout << "Nested Loop Join results (left|right):\n";
    while (nlj.next(out)) {
        cout << " - " << string(out.begin(), out.end()) << "\n";
    }
    nlj.close();

    // Construir B+ Tree sobre right table: clave -> RID
    BPlusTree bpt(&bufferManager, &storageManager, 3, 3);
    // recorrer right table y agregar claves
    Scan rightForIndex(&bufferManager, 20, rightLastPage);
    rightForIndex.open();
    vector<uint8_t> rec;
    int insertCount = 0;
    int pageCursor = 20;
    while (rightForIndex.next(rec)) {
        // find RID by scanning pages again is complex; instead we approximate by searching pages for record
        index_m::RID rid;
        // Naive: scan pages to find the record's slot - we'll search pages sequentially
        bool found = false;
        for (int pid = 20; pid <= rightLastPage && !found; ++pid) {
            Frame* f = bufferManager.GetPage(pid);
            for (int s = 0; s < f->page->GetNumSlots(); ++s) {
                auto bytes = f->page->ReadRecord(s);
                if (bytes == rec) {
                    rid.pageId = pid;
                    rid.slotId = s;
                    found = true;
                    break;
                }
            }
            bufferManager.ReleasePage(pid, false);
        }
        int key = ParseKey(rec);
        if (rid.IsValid()) {
            bpt.Insert(key, rid);
            insertCount++;
        }
    }
    rightForIndex.close();

    cout << "B+ Tree construido con " << insertCount << " entradas." << endl;

    // Index-based join: for each left tuple, lookup in B+ Tree
    Scan leftScan2(&bufferManager, 1, leftLastPage);
    leftScan2.open();
    cout << "Index-based join results (left|right_via_index):\n";
    while (leftScan2.next(rec)) {
        int k = ParseKey(rec);
        index_m::RID found = bpt.Search(k);
        if (found.IsValid()) {
            Frame* f = bufferManager.GetPage(found.pageId);
            auto rightRec = f->page->ReadRecord(found.slotId);
            bufferManager.ReleasePage(found.pageId, false);
            // output concatenation
            string outstr = string(rec.begin(), rec.end()) + "|" + string(rightRec.begin(), rightRec.end());
            cout << " - " << outstr << "\n";
        }
    }
    leftScan2.close();

    cout << "Demo join finalizado." << endl;
    return 0;
}

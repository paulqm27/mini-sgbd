#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <cstdio>

#include "../src/storage/storage.h"
#include "../src/buffer/buffer.h"
#include "../src/index/bplus_tree.h"
#include "../src/execution/scan.h"
#include "../src/execution/select.h"
#include "../src/execution/index_scan.h"
#include "../src/execution/planner.h"

using namespace std;
using namespace storage;
using namespace buffer;
using namespace index_m;
using namespace exec;

void InsertRecords(BufferManager& bm, int& pageId, const vector<int>& keys, const string& label) {
    for (int k : keys) {
        string payload = label + " K:" + to_string(k);
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
}

int ParseKey(const vector<uint8_t>& rec) {
    string s(rec.begin(), rec.end());
    auto pos = s.find("K:");
    if (pos == string::npos) return -1;
    try { return stoi(s.substr(pos + 2)); } catch(...) { return -1; }
}

int main() {
    const string db = "data/benchmark_index.db";
    remove(db.c_str());

    StorageManager storageManager(db);
    BufferManager bufferManager(20, &storageManager);

    // Generate data
    vector<int> keys;
    for (int i = 1; i <= 100; ++i) keys.push_back(i);

    int dataPage = 1;
    InsertRecords(bufferManager, dataPage, keys, "REC");
    bufferManager.Flush();

    // Build B+ Tree index
    BPlusTree bpt(&bufferManager, &storageManager, 3, 3);
    for (int pid = 1; pid <= dataPage; ++pid) {
        Frame* f = bufferManager.GetPage(pid);
        for (int slot = 0; slot < f->page->GetNumSlots(); ++slot) {
            auto rec = f->page->ReadRecord(slot);
            int key = ParseKey(rec);
            if (key >= 0) {
                index_m::RID rid{pid, slot};
                bpt.Insert(key, rid);
            }
        }
        bufferManager.ReleasePage(pid, false);
    }
    bufferManager.Flush();

    cout << "Benchmark: Scan vs IndexScan\n";
    cout << "Datos: 100 registros en paginas 1.." << dataPage << "\n";
    cout << "search_key,scan_accesses,scan_misses,index_accesses,index_misses,scan_ms,index_ms\n";

    vector<int> searchKeys = {5, 25, 50, 75, 99};
    for (int key : searchKeys) {
        // Scan approach
        bufferManager.ResetStats();
        auto t0 = chrono::high_resolution_clock::now();
        {
            Scan scan(&bufferManager, 1, dataPage);
            auto pred = [key](const vector<uint8_t>& rec) { return ParseKey(rec) == key; };
            Select sel(&scan, pred);
            sel.open();
            vector<uint8_t> out;
            while (sel.next(out)) {}
            sel.close();
        }
        auto t1 = chrono::high_resolution_clock::now();
        double scan_ms = chrono::duration<double, milli>(t1 - t0).count();
        int scan_acc = bufferManager.GetAccessCount();
        int scan_miss = bufferManager.GetMissCount();

        // IndexScan approach
        bufferManager.ResetStats();
        t0 = chrono::high_resolution_clock::now();
        {
            PredicateInfo info{true, key};
            if (Planner::UseIndexScan(&bpt, info)) {
                IndexScan idxScan(&bpt, &bufferManager, key);
                idxScan.open();
                vector<uint8_t> out;
                while (idxScan.next(out)) {}
                idxScan.close();
            }
        }
        t1 = chrono::high_resolution_clock::now();
        double index_ms = chrono::duration<double, milli>(t1 - t0).count();
        int index_acc = bufferManager.GetAccessCount();
        int index_miss = bufferManager.GetMissCount();

        cout << key << "," << scan_acc << "," << scan_miss << "," << index_acc << "," << index_miss << "," << scan_ms << "," << index_ms << "\n";
    }

    return 0;
}

#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <cstdio>

#include "../src/storage/storage.h"
#include "../src/buffer/buffer.h"

using namespace std;
using namespace storage;
using namespace buffer;

struct TableRID { int pageId; int slotId; };

void InsertRecords(BufferManager& bm, int& currentDataPageId, const vector<string>& records) {
    for (const auto& s : records) {
        vector<uint8_t> data(s.begin(), s.end());
        Frame* frame = bm.GetPage(currentDataPageId);
        if (!frame->page->InsertRecord(data)) {
            bm.ReleasePage(currentDataPageId, false);
            currentDataPageId++;
            frame = bm.GetPage(currentDataPageId);
            frame->page->InsertRecord(data);
        }
        bm.ReleasePage(currentDataPageId, true);
    }
}

int main() {
    const string db = "data/buffer_benchmark.db";
    remove(db.c_str());

    StorageManager storageManager(db);

    // Create a small loader BufferManager to populate data
    BufferManager loader(50, &storageManager);
    int currentPage = 1;

    // Generate many small records to fill pages
    vector<string> recs;
    const int NUM_RECORDS = 2000;
    for (int i = 0; i < NUM_RECORDS; ++i) {
        recs.push_back("REC_" + to_string(i) + "_DATA_");
    }
    InsertRecords(loader, currentPage, recs);
    loader.Flush();
    int lastPage = currentPage;
    cout << "Inserted " << NUM_RECORDS << " records into pages 1.." << lastPage << "\n";

    // Benchmark parameters
    vector<int> bufferSizes = {1, 2, 5, 10, 20, 50};
    int iterations = 5; // number of full scans per buffer size

    cout << "buffer_size,accesses,hits,misses,hit_rate,time_ms" << endl;

    for (int bufSize : bufferSizes) {
        BufferManager bm(bufSize, &storageManager);
        bm.ResetStats();
        auto t0 = chrono::high_resolution_clock::now();

        for (int it = 0; it < iterations; ++it) {
            for (int pid = 1; pid <= lastPage; ++pid) {
                Frame* f = bm.GetPage(pid);
                // simulate light processing
                if (f && f->page) {
                    volatile int nslots = f->page->GetNumSlots(); (void)nslots;
                }
                bm.ReleasePage(pid, false);
            }
        }

        auto t1 = chrono::high_resolution_clock::now();
        double ms = chrono::duration<double, milli>(t1 - t0).count();

        int accesses = bm.GetAccessCount();
        int hits = bm.GetHitCount();
        int misses = bm.GetMissCount();
        double hitRate = bm.GetHitRate();

        cout << bufSize << "," << accesses << "," << hits << "," << misses << "," << hitRate << "," << ms << endl;
    }

    return 0;
}

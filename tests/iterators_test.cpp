#include <iostream>
#include <vector>
#include <string>
#include <cstdio>

#include "../src/storage/storage.h"
#include "../src/buffer/buffer.h"
#include "../src/execution/scan.h"
#include "../src/execution/select.h"
#include "../src/execution/project.h"

using namespace std;
using namespace storage;
using namespace buffer;
using namespace exec;

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
    const string db = "data/iterators_test.db";
    remove(db.c_str());

    StorageManager storageManager(db);
    BufferManager bm(10, &storageManager);

    int currentPage = 1;
    vector<string> recs = {"a1","b2","c3","d4","e5"};
    InsertRecords(bm, currentPage, recs);
    bm.Flush();

    Scan scan(&bm, 1, currentPage);
    auto pred = [](const vector<uint8_t>& v) { string s(v.begin(), v.end()); return s.find('3')!=string::npos || s.find('4')!=string::npos; };
    auto proj = [](const vector<uint8_t>& v) { string s(v.begin(), v.end()); return vector<uint8_t>(s.begin(), s.end()); };
    Select sel(&scan, pred);
    Project p(&sel, proj);

    p.open();
    vector<uint8_t> out; int count=0;
    while (p.next(out)) { count++; }
    p.close();

    if (count != 2) {
        cerr << "Iterators test failed: expected 2, got " << count << "\n";
        return 1;
    }
    cout << "Iterators test passed." << endl;
    return 0;
}

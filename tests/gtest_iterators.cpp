#include "gtest/gtest.h"
#include "../src/storage/storage.h"
#include "../src/buffer/buffer.h"
#include "../src/execution/scan.h"
#include "../src/execution/select.h"
#include "../src/execution/project.h"
#include "../src/execution/planner.h"

using namespace storage;
using namespace buffer;
using namespace exec;

static void InsertRecords(BufferManager& bm, int& currentDataPageId, const std::vector<std::string>& records) {
    for (const auto& s : records) {
        std::vector<uint8_t> data(s.begin(), s.end());
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

TEST(IteratorsGTest, ScanSelectProject) {
    const std::string db = "data/gtest_iter.db";
    remove(db.c_str());
    StorageManager storageManager(db);
    BufferManager bm(10, &storageManager);
    int cur = 1;
    InsertRecords(bm, cur, {"x1","x2","x3","x4"});
    bm.Flush();

    Scan scan(&bm, 1, cur);
    auto pred = [](const std::vector<uint8_t>& v){ return std::string(v.begin(), v.end()).find('3')!=std::string::npos; };
    Select sel(&scan, pred);
    Project p(&sel, [](const std::vector<uint8_t>& v){ return v; });

    p.open();
    std::vector<uint8_t> out; int cnt=0;
    while (p.next(out)) cnt++;
    p.close();
    EXPECT_EQ(cnt, 1);
}

TEST(PlannerGTest, UseIndexScanHeuristic) {
    // Planner should choose IndexScan for equality predicate when tree present
    exec::PredicateInfo pi;
    pi.is_equality = true; pi.key = 42;
    EXPECT_FALSE(Planner::UseIndexScan(nullptr, pi));
    // create dummy StorageManager and BufferManager to construct a BPlusTree
    const std::string db = "data/gtest_planner.db";
    remove(db.c_str());
    StorageManager storageManager(db);
    BufferManager bm(10, &storageManager);
    index_m::BPlusTree bpt(&bm, &storageManager, 3, 3);
    EXPECT_TRUE(Planner::UseIndexScan(&bpt, pi));
}

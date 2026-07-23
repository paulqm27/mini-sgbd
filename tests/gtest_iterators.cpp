#include "gtest/gtest.h"
#include "../src/storage/storage.h"
#include "../src/buffer/buffer.h"
#include "../src/execution/scan.h"
#include "../src/execution/select.h"
#include "../src/execution/project.h"
#include "../src/execution/nested_loop_join.h"
#include "../src/execution/index_scan.h"
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

TEST(SelectGTest, FilteredResult) {
    const std::string db = "data/gtest_select.db";
    remove(db.c_str());
    StorageManager storageManager(db);
    BufferManager bm(10, &storageManager);
    int cur = 1;
    InsertRecords(bm, cur, {"apple", "banana", "apricot", "cherry"});
    bm.Flush();

    Scan scan(&bm, 1, cur);
    auto pred = [](const std::vector<uint8_t>& v){ return std::string(v.begin(), v.end())[0] == 'a'; };
    Select sel(&scan, pred);

    sel.open();
    std::vector<uint8_t> out; int cnt=0;
    while (sel.next(out)) cnt++;
    sel.close();
    EXPECT_EQ(cnt, 2); // apple, apricot
}

TEST(ProjectGTest, TruncateProjection) {
    const std::string db = "data/gtest_project.db";
    remove(db.c_str());
    StorageManager storageManager(db);
    BufferManager bm(10, &storageManager);
    int cur = 1;
    InsertRecords(bm, cur, {"verylongstring", "short"});
    bm.Flush();

    Scan scan(&bm, 1, cur);
    auto proj = [](const std::vector<uint8_t>& v){
        if (v.size() > 5) {
            return std::vector<uint8_t>(v.begin(), v.begin() + 5);
        }
        return v;
    };
    Project p(&scan, proj);

    p.open();
    std::vector<uint8_t> out;
    EXPECT_TRUE(p.next(out));
    EXPECT_EQ(out.size(), 5);
    p.close();
}

TEST(NestedLoopJoinGTest, JoinEquality) {
    const std::string db = "data/gtest_join.db";
    remove(db.c_str());
    StorageManager storageManager(db);
    BufferManager bm(10, &storageManager);
    int left_p = 1, right_p = 10;
    InsertRecords(bm, left_p, {"L1", "L2", "L3"});
    InsertRecords(bm, right_p, {"R1", "R2", "R3"});
    bm.Flush();

    Scan leftScan(&bm, 1, left_p);
    Scan rightScan(&bm, 10, right_p);
    auto pred = [](const std::vector<uint8_t>& l, const std::vector<uint8_t>& r){
        return std::string(l.begin(), l.end())[1] == std::string(r.begin(), r.end())[1];
    };
    exec::NestedLoopJoin nlj(&leftScan, &rightScan, pred);

    nlj.open();
    std::vector<uint8_t> out; int cnt=0;
    while (nlj.next(out)) cnt++;
    nlj.close();
    EXPECT_EQ(cnt, 3); // L1|R1, L2|R2, L3|R3
}

TEST(IndexScanGTest, ExactKeySearch) {
    const std::string db = "data/gtest_indexscan.db";
    remove(db.c_str());
    StorageManager storageManager(db);
    BufferManager bm(10, &storageManager);

    // Insert keyed records
    int page_id = 1;
    for (int k : {10, 20, 30}) {
        std::string s = "K:" + std::to_string(k);
        std::vector<uint8_t> data(s.begin(), s.end());
        Frame* f = bm.GetPage(page_id);
        f->page->InsertRecord(data);
        bm.ReleasePage(page_id, true);
    }
    bm.Flush();

    // Build B+ Tree
    index_m::BPlusTree bpt(&bm, &storageManager, 3, 3);
    for (int k : {10, 20, 30}) {
        index_m::RID rid{1, static_cast<int32_t>(k / 10 - 1)};
        bpt.Insert(k, rid);
    }
    bm.Flush();

    // Search for key 20
    exec::IndexScan idxScan(&bpt, &bm, 20);
    idxScan.open();
    std::vector<uint8_t> out;
    bool found = idxScan.next(out);
    idxScan.close();
    EXPECT_TRUE(found);
    std::string result(out.begin(), out.end());
    EXPECT_TRUE(result.find("20") != std::string::npos || result.find("K:2") != std::string::npos);
}

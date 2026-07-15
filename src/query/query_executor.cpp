#include "query_executor.h"

#include "scan_operator.h"
#include "select_operator.h"
#include "project_operator.h"
#include "index_scan_operator.h"

#include <iostream>
#include <cassert>

namespace query {

    QueryExecutor::QueryExecutor(buffer::BufferManager*   bufferManager,
                                 storage::StorageManager* storageManager,
                                 index_m::BPlusTree*      bPlusTree)
        : bufferManager_(bufferManager)
        , storageManager_(storageManager)
        , bPlusTree_(bPlusTree)
    {}

    bool QueryExecutor::CanUseIndex(const QueryPlan& plan) const {
        return (bPlusTree_ != nullptr)
            && (plan.predicate.type == Predicate::Type::EQ_ID);
    }

    std::unique_ptr<Iterator> QueryExecutor::BuildPlan(const QueryPlan& plan) const {
        std::unique_ptr<Iterator> root;

        if (CanUseIndex(plan)) {
            root = std::make_unique<IndexScanOperator>(
                bPlusTree_,
                bufferManager_,
                plan.predicate.idValue
            );
        } else {
            auto scan = std::make_unique<ScanOperator>(
                bufferManager_,
                storageManager_,
                plan.startPage
            );

            if (plan.predicate.type == Predicate::Type::GENERAL) {
                root = std::make_unique<SelectOperator>(
                    scan.release(),
                    plan.predicate.generalFn
                );
            } else {
                root = std::move(scan);
            }
        }
        if (!plan.columns.empty()) {
            root = std::make_unique<ProjectOperator>(
                root.release(),
                plan.columns
            );
        }

        return root;
    }

    QueryResult QueryExecutor::Execute(const QueryPlan& plan) {
        QueryResult result;
        int accessesBefore = bufferManager_->GetAccessCount();

        auto rootIterator = BuildPlan(plan);
        rootIterator->Open();

        Record rec;
        while (rootIterator->Next(rec)) {
            result.records.push_back(rec);
            result.totalScanned++;
        }

        rootIterator->Close();

        result.hitRate      = bufferManager_->GetHitRate();
        result.pageAccesses = bufferManager_->GetAccessCount() - accessesBefore;

        return result;
    }

    void QueryExecutor::Explain(const QueryPlan& plan) const {
        std::cout << "\n[QueryExecutor::Explain]" << std::endl;

        if (CanUseIndex(plan)) {
            std::cout << "  Plan seleccionado : INDEX SCAN" << std::endl;
            std::cout << "  Operadores        : IndexScanOperator";
            if (!plan.columns.empty()) std::cout << " → ProjectOperator";
            std::cout << std::endl;
            std::cout << "  Clave buscada     : id = " << plan.predicate.idValue << std::endl;
        } else {
            std::cout << "  Plan seleccionado : FULL TABLE SCAN" << std::endl;
            std::cout << "  Operadores        : ScanOperator";
            if (plan.predicate.type == Predicate::Type::GENERAL) {
                std::cout << " → SelectOperator";
            }
            if (!plan.columns.empty()) std::cout << " → ProjectOperator";
            std::cout << std::endl;
        }

        std::cout << "  Índice disponible : " << (bPlusTree_ ? "SI" : "NO") << std::endl;
        std::cout << "  Proyección        : "
                  << (plan.columns.empty() ? "ninguna (SELECT *)"
                                           : std::to_string(plan.columns.size()) + " columnas")
                  << std::endl;
    }

} // namespace query

#pragma once

#include "iterator.h"
#include "../index/bplus_tree.h"

namespace exec {

    struct PredicateInfo {
        bool is_equality = false; // equality predicate on a single column
        int key = 0; // for equality, the key value
    };

    // Planner: decide entre IndexScan o Scan según disponibilidad de índice
    class Planner {
    public:
        // Si hay un índice y la predicado es igualdad sobre la columna indexada,
        // devolverá true para usar IndexScan; en otro caso false para usar Scan.
        static bool UseIndexScan(index_m::BPlusTree* tree, const PredicateInfo& pred);
    };

}

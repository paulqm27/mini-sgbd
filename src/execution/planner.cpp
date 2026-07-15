#include "planner.h"

namespace exec {

    bool Planner::UseIndexScan(index_m::BPlusTree* tree, const PredicateInfo& pred) {
        if (!tree) return false;
        // heurística simple: usar índice solo para igualdad simple
        return pred.is_equality;
    }

}

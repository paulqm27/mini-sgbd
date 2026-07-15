#include "project.h"

namespace exec {

    Project::Project(Iterator* child, Proj proj)
        : child_(child), proj_(proj) {}

    void Project::open() { if (child_) child_->open(); }

    bool Project::next(std::vector<uint8_t>& out) {
        std::vector<uint8_t> in;
        if (!child_ || !child_->next(in)) return false;
        out = proj_(in);
        return true;
    }

    void Project::close() { if (child_) child_->close(); }

    Project::~Project() { /* no ownership */ }

}

#pragma once

#include "record.h"

namespace query {
    class Iterator {
    public:
        virtual void Open()              = 0;
        virtual bool Next(Record& record) = 0;
        virtual void Close()             = 0;
        virtual ~Iterator() {}
    };

} // namespace query

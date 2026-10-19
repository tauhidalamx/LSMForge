#pragma once

#include "db/version_set.h"
#include "lsmforge/status.h"
#include <atomic>

namespace lsmforge {

class DBImpl;

class CompactionEngine {
public:
    static Status DoCompactionWork(DBImpl* db, Compaction* c);
};

} // namespace lsmforge

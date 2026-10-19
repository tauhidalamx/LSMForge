#pragma once

#include "lsmforge/types.h"

namespace lsmforge {

class Snapshot {
public:
    virtual ~Snapshot() = default;

    [[nodiscard]] virtual SequenceNumber GetSequenceNumber() const = 0;
};

} // namespace lsmforge

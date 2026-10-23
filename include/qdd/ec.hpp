#pragma once

#include "qdd/circuit.hpp"
#include "qdd/dd.hpp"
#include "qdd/stats.hpp"

#include <string>

namespace qdd {

enum class ECStrategy {
    Alternating,
    Naive
};

enum class ECStatus {
    Equivalent,
    EquivalentUpToGlobalPhase,
    NotEquivalent,
    QubitMismatch,
    NodeLimitExceeded,
    Timeout
};

struct ECResult {
    ECStatus status = ECStatus::NotEquivalent;
    double global_phase = 0.0;
    Stats stats{};
    double runtime_seconds = 0.0;
    std::size_t peak_memory_bytes = 0;
};

class EquivalenceChecker {
public:
    explicit EquivalenceChecker(PackageConfig cfg = {}, ECStrategy strategy = ECStrategy::Alternating)
        : cfg_(cfg), strategy_(strategy) {}

    ECResult check(const Circuit& c1, const Circuit& c2);

private:
    PackageConfig cfg_;
    ECStrategy strategy_;

    bool quick_refute(Package& pkg, const Circuit& c1, const Circuit& c2, int num_trials = 5);
    Edge build_gate(Package& pkg, const GateOp& op);
};

} // namespace qdd

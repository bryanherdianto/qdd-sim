#pragma once

#include "qdd/circuit.hpp"
#include "qdd/dd.hpp"
#include "qdd/stats.hpp"

#include <chrono>
#include <memory>

namespace qdd {

struct SimulationResult {
    Edge final_state{};
    Stats stats{};
    double runtime_seconds = 0.0;
    std::size_t peak_memory_bytes = 0;
    std::shared_ptr<Package> pkg;

    std::complex<double> amplitude(std::uint64_t basis) const {
        return pkg ? pkg->amplitude(final_state, basis) : std::complex<double>(0.0, 0.0);
    }
};

class Simulator {
public:
    explicit Simulator(PackageConfig cfg = {}) : cfg_(cfg) {}

    SimulationResult simulate(const Circuit& circuit);

private:
    PackageConfig cfg_;
    Edge build_gate(Package& pkg, const GateOp& op);
};

} // namespace qdd

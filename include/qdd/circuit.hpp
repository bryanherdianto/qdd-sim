#pragma once

#include "qdd/dd.hpp"
#include <string>
#include <vector>

namespace qdd {

enum class GateType {
    X, Y, Z, H, S, Sdg, T, Tdg, SX,
    RX, RY, RZ, P, U1, U2, U3,
    CX, CY, CZ, CH, CP, CRX, CRY, CRZ,
    SWAP, CCX, CSWAP, MCX,
    Barrier, Measure
};

struct GateOp {
    GateType type;
    std::vector<std::uint16_t> targets;
    std::vector<Control> controls;
    std::vector<double> params;
};

struct Circuit {
    std::size_t nqubits = 0;
    std::vector<std::pair<std::string, std::size_t>> qregs;
    std::vector<std::pair<std::string, std::size_t>> cregs;
    std::vector<GateOp> ops;

    std::uint16_t get_qubit_index(const std::string& reg, std::size_t idx) const {
        std::uint16_t base = 0;
        for (const auto& [name, size] : qregs) {
            if (name == reg) {
                if (idx >= size) throw std::out_of_range("Qubit index out of range for register " + reg);
                return static_cast<std::uint16_t>(base + idx);
            }
            base = static_cast<std::uint16_t>(base + size);
        }
        throw std::runtime_error("Unknown quantum register: " + reg);
    }
};

} // namespace qdd

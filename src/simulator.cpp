#include "qdd/simulator.hpp"
#include "qdd/gates.hpp"
#include "qdd/memory_usage.hpp"

#include <chrono>

namespace qdd {

SimulationResult Simulator::simulate(const Circuit& circuit) {
    auto start_time = std::chrono::high_resolution_clock::now();

    auto pkg_ptr = std::make_shared<Package>(circuit.nqubits, cfg_);
    Package& pkg = *pkg_ptr;
    Package::Root psi(pkg, pkg.zero_state());

    for (const auto& op : circuit.ops) {
        if (op.type == GateType::Barrier || op.type == GateType::Measure) {
            continue;
        }

        if (op.type == GateType::SWAP) {
            std::uint16_t a = op.targets[0];
            std::uint16_t b = op.targets[1];
            Edge g1 = pkg.gate(gates::X(), b, {Control{a, true}});
            psi.set(pkg.mat_vec(g1, psi.edge()));
            Edge g2 = pkg.gate(gates::X(), a, {Control{b, true}});
            psi.set(pkg.mat_vec(g2, psi.edge()));
            Edge g3 = pkg.gate(gates::X(), b, {Control{a, true}});
            psi.set(pkg.mat_vec(g3, psi.edge()));
            pkg.check_gc();
            continue;
        }

        if (op.type == GateType::CSWAP) {
            std::uint16_t c = op.controls[0].qubit;
            std::uint16_t a = op.targets[0];
            std::uint16_t b = op.targets[1];
            Edge g1 = pkg.gate(gates::X(), a, {Control{b, true}});
            psi.set(pkg.mat_vec(g1, psi.edge()));
            Edge g2 = pkg.gate(gates::X(), b, {Control{c, true}, Control{a, true}});
            psi.set(pkg.mat_vec(g2, psi.edge()));
            Edge g3 = pkg.gate(gates::X(), a, {Control{b, true}});
            psi.set(pkg.mat_vec(g3, psi.edge()));
            pkg.check_gc();
            continue;
        }

        Edge g = build_gate(pkg, op);
        psi.set(pkg.mat_vec(g, psi.edge()));
        pkg.check_gc();
    }

    pkg.gc();

    auto end_time = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration<double>(end_time - start_time).count();

    SimulationResult res;
    res.final_state = psi.edge();
    res.stats = pkg.stats();
    res.runtime_seconds = duration;
    res.peak_memory_bytes = get_peak_rss_bytes();
    res.pkg = pkg_ptr;
    return res;
}

Edge Simulator::build_gate(Package& pkg, const GateOp& op) {
    std::uint16_t target = op.targets[0];
    const auto& ctrls = op.controls;

    switch (op.type) {
        case GateType::X: case GateType::CX: case GateType::CCX: case GateType::MCX:
            return pkg.gate(gates::X(), target, ctrls);
        case GateType::Y: case GateType::CY:
            return pkg.gate(gates::Y(), target, ctrls);
        case GateType::Z: case GateType::CZ:
            return pkg.gate(gates::Z(), target, ctrls);
        case GateType::H: case GateType::CH:
            return pkg.gate(gates::H(), target, ctrls);
        case GateType::S:
            return pkg.gate(gates::S(), target, ctrls);
        case GateType::Sdg:
            return pkg.gate(gates::Sdg(), target, ctrls);
        case GateType::T:
            return pkg.gate(gates::T(), target, ctrls);
        case GateType::Tdg:
            return pkg.gate(gates::Tdg(), target, ctrls);
        case GateType::SX:
            return pkg.gate(gates::SX(), target, ctrls);
        case GateType::RX: case GateType::CRX:
            return pkg.gate(gates::RX(op.params[0]), target, ctrls);
        case GateType::RY: case GateType::CRY:
            return pkg.gate(gates::RY(op.params[0]), target, ctrls);
        case GateType::RZ: case GateType::CRZ:
            return pkg.gate(gates::RZ(op.params[0]), target, ctrls);
        case GateType::P: case GateType::U1: case GateType::CP:
            return pkg.gate(gates::P(op.params[0]), target, ctrls);
        case GateType::U2:
            return pkg.gate(gates::U2(op.params[0], op.params[1]), target, ctrls);
        case GateType::U3:
            return pkg.gate(gates::U3(op.params[0], op.params[1], op.params[2]), target, ctrls);
        default:
            return pkg.identity();
    }
}

} // namespace qdd

#include "qdd/ec.hpp"
#include "qdd/gates.hpp"
#include "qdd/memory_usage.hpp"

#include <chrono>
#include <cmath>
#include <numbers>
#include <random>

namespace qdd {

static std::complex<double> inner_product_rec(Package& pkg, Edge u, Edge v) {
    if (u.is_zero() || v.is_zero()) return {0.0, 0.0};
    if (u.is_terminal() && v.is_terminal()) {
        auto val_u = pkg.complex_table().value(u.w);
        auto val_v = pkg.complex_table().value(v.w);
        return std::conj(val_u) * val_v;
    }

    std::uint16_t qu = (u.node != kTerminal) ? pkg.vnode(u.node).qubit : 0;
    std::uint16_t qv = (v.node != kTerminal) ? pkg.vnode(v.node).qubit : 0;
    std::uint16_t q = std::max(qu, qv);

    Edge u0{}, u1{};
    if (u.node != kTerminal && qu == q) {
        const auto& un = pkg.vnode(u.node);
        u0 = Edge{un.e[0].node, pkg.complex_table().mul(u.w, un.e[0].w)};
        u1 = Edge{un.e[1].node, pkg.complex_table().mul(u.w, un.e[1].w)};
    } else {
        u0 = u;
        u1 = Edge{kTerminal, kC0};
    }

    Edge v0{}, v1{};
    if (v.node != kTerminal && qv == q) {
        const auto& vn = pkg.vnode(v.node);
        v0 = Edge{vn.e[0].node, pkg.complex_table().mul(v.w, vn.e[0].w)};
        v1 = Edge{vn.e[1].node, pkg.complex_table().mul(v.w, vn.e[1].w)};
    } else {
        v0 = v;
        v1 = Edge{kTerminal, kC0};
    }

    return inner_product_rec(pkg, u0, v0) + inner_product_rec(pkg, u1, v1);
}

bool EquivalenceChecker::quick_refute(Package& pkg, const Circuit& c1, const Circuit& c2, int num_trials) {
    std::mt19937_64 rng(1337);

    for (int t = 0; t < num_trials; ++t) {
        std::uint64_t mask = 0;
        if (c1.nqubits < 64) {
            std::uniform_int_distribution<std::uint64_t> dist(0, (1ULL << c1.nqubits) - 1);
            mask = dist(rng);
        } else {
            mask = rng();
        }

        Package::Root psi1(pkg, pkg.basis_state(mask));
        Package::Root psi2(pkg, pkg.basis_state(mask));

        for (const auto& op : c1.ops) {
            if (op.type == GateType::Barrier || op.type == GateType::Measure) continue;
            Edge g = build_gate(pkg, op);
            psi1.set(pkg.mat_vec(g, psi1.edge()));
        }

        for (const auto& op : c2.ops) {
            if (op.type == GateType::Barrier || op.type == GateType::Measure) continue;
            Edge g = build_gate(pkg, op);
            psi2.set(pkg.mat_vec(g, psi2.edge()));
        }

        auto ip = inner_product_rec(pkg, psi1.edge(), psi2.edge());
        double fid = std::norm(ip); // |<psi1|psi2>|^2
        if (fid < 1.0 - 1e-6) {
            return true; // Refuted! Not equivalent.
        }
    }
    return false;
}

Edge EquivalenceChecker::build_gate(Package& pkg, const GateOp& op) {
    if (op.type == GateType::SWAP) {
        std::uint16_t a = op.targets[0];
        std::uint16_t b = op.targets[1];
        Edge g1 = pkg.gate(gates::X(), b, {Control{a, true}});
        Edge g2 = pkg.gate(gates::X(), a, {Control{b, true}});
        Edge g3 = pkg.gate(gates::X(), b, {Control{a, true}});
        return pkg.mat_mat(g3, pkg.mat_mat(g2, g1));
    }

    if (op.type == GateType::CSWAP) {
        std::uint16_t c = op.controls[0].qubit;
        std::uint16_t a = op.targets[0];
        std::uint16_t b = op.targets[1];
        Edge g1 = pkg.gate(gates::X(), a, {Control{b, true}});
        Edge g2 = pkg.gate(gates::X(), b, {Control{c, true}, Control{a, true}});
        Edge g3 = pkg.gate(gates::X(), a, {Control{b, true}});
        return pkg.mat_mat(g3, pkg.mat_mat(g2, g1));
    }

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

ECResult EquivalenceChecker::check(const Circuit& c1, const Circuit& c2) {
    auto start_time = std::chrono::high_resolution_clock::now();

    if (c1.nqubits != c2.nqubits) {
        ECResult res;
        res.status = ECStatus::QubitMismatch;
        return res;
    }

    Package pkg(c1.nqubits, cfg_);

    // Quick refutation via simulation
    if (quick_refute(pkg, c1, c2, 5)) {
        auto end_time = std::chrono::high_resolution_clock::now();
        ECResult res;
        res.status = ECStatus::NotEquivalent;
        res.stats = pkg.stats();
        res.runtime_seconds = std::chrono::duration<double>(end_time - start_time).count();
        res.peak_memory_bytes = get_peak_rss_bytes();
        return res;
    }

    // Filter non-gate ops
    std::vector<GateOp> ops1, ops2;
    for (const auto& op : c1.ops) {
        if (op.type != GateType::Barrier && op.type != GateType::Measure) ops1.push_back(op);
    }
    for (const auto& op : c2.ops) {
        if (op.type != GateType::Barrier && op.type != GateType::Measure) ops2.push_back(op);
    }

    Package::Root E(pkg, pkg.identity());

    try {
        if (strategy_ == ECStrategy::Alternating) {
            std::size_t i1 = 0, i2 = 0;
            std::size_t N1 = ops1.size();
            std::size_t N2 = ops2.size();

            while (i1 < N1 || i2 < N2) {
                if (i1 < N1 && (i2 == N2 || (i1 + 1) * N2 <= (i2 + 1) * N1)) {
                    Edge g = build_gate(pkg, ops1[i1++]);
                    E.set(pkg.mat_mat(g, E.edge()));
                } else if (i2 < N2) {
                    Edge h = build_gate(pkg, ops2[i2++]);
                    Edge h_adj = pkg.adjoint(h);
                    E.set(pkg.mat_mat(E.edge(), h_adj));
                }
            }
        } else {
            // Naive strategy: build U1, U2, then E = U1 * U2^dagger
            Package::Root U1(pkg, pkg.identity());
            for (const auto& op : ops1) {
                Edge g = build_gate(pkg, op);
                U1.set(pkg.mat_mat(g, U1.edge()));
            }

            Package::Root U2(pkg, pkg.identity());
            for (const auto& op : ops2) {
                Edge g = build_gate(pkg, op);
                U2.set(pkg.mat_mat(g, U2.edge()));
            }

            Edge U2_adj = pkg.adjoint(U2.edge());
            E.set(pkg.mat_mat(U1.edge(), U2_adj));
        }
    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
        ECResult res;
        if (msg.find("Node limit") != std::string::npos) {
            res.status = ECStatus::NodeLimitExceeded;
        } else {
            throw;
        }
        res.stats = pkg.stats();
        res.peak_memory_bytes = get_peak_rss_bytes();
        return res;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration<double>(end_time - start_time).count();

    ECResult res;
    res.stats = pkg.stats();
    res.runtime_seconds = duration;
    res.peak_memory_bytes = get_peak_rss_bytes();

    Edge final_e = E.edge();
    bool is_id = false;
    if (final_e.node != kTerminal) {
        if (pkg.mnode(final_e.node).identity || final_e.node == pkg.identity().node) {
            is_id = true;
        }
    }

    if (is_id) {
        auto val = pkg.complex_table().value(final_e.w);
        double mag = std::abs(val);
        if (std::abs(mag - 1.0) < 1e-6) {
            double theta = std::arg(val);
            if (std::abs(theta) < 1e-9 || std::abs(std::abs(theta) - 2.0 * std::numbers::pi) < 1e-9) {
                res.status = ECStatus::Equivalent;
                res.global_phase = 0.0;
            } else {
                res.status = ECStatus::EquivalentUpToGlobalPhase;
                res.global_phase = theta;
            }
            return res;
        }
    }

    res.status = ECStatus::NotEquivalent;
    return res;
}

} // namespace qdd

#include "qdd/gates.hpp"

#include <cassert>
#include <cmath>
#include <numbers>

namespace qdd {

namespace gates {

Mat2 I() {
    return {{{1.0, 0.0}, {0.0, 1.0}}};
}

Mat2 X() {
    return {{{0.0, 1.0}, {1.0, 0.0}}};
}

Mat2 Y() {
    return {{{0.0, std::complex<double>(0.0, -1.0)},
             {std::complex<double>(0.0, 1.0), 0.0}}};
}

Mat2 Z() {
    return {{{1.0, 0.0}, {0.0, -1.0}}};
}

Mat2 H() {
    const double inv = 1.0 / std::sqrt(2.0);
    return {{{inv, inv}, {inv, -inv}}};
}

Mat2 S() {
    return {{{1.0, 0.0}, {0.0, std::complex<double>(0.0, 1.0)}}};
}

Mat2 Sdg() {
    return {{{1.0, 0.0}, {0.0, std::complex<double>(0.0, -1.0)}}};
}

Mat2 T() {
    const double inv = 1.0 / std::sqrt(2.0);
    return {{{1.0, 0.0}, {0.0, std::complex<double>(inv, inv)}}};
}

Mat2 Tdg() {
    const double inv = 1.0 / std::sqrt(2.0);
    return {{{1.0, 0.0}, {0.0, std::complex<double>(inv, -inv)}}};
}

Mat2 SX() {
    return {{{std::complex<double>(0.5, 0.5), std::complex<double>(0.5, -0.5)},
             {std::complex<double>(0.5, -0.5), std::complex<double>(0.5, 0.5)}}};
}

Mat2 RX(double theta) {
    double c = std::cos(theta / 2.0);
    double s = std::sin(theta / 2.0);
    return {{{c, std::complex<double>(0.0, -s)},
             {std::complex<double>(0.0, -s), c}}};
}

Mat2 RY(double theta) {
    double c = std::cos(theta / 2.0);
    double s = std::sin(theta / 2.0);
    return {{{c, -s}, {s, c}}};
}

Mat2 RZ(double theta) {
    return {{{std::exp(std::complex<double>(0.0, -theta / 2.0)), 0.0},
             {0.0, std::exp(std::complex<double>(0.0, theta / 2.0))}}};
}

Mat2 P(double lambda) {
    return {{{1.0, 0.0}, {0.0, std::exp(std::complex<double>(0.0, lambda))}}};
}

Mat2 U1(double lambda) {
    return P(lambda);
}

Mat2 U2(double phi, double lambda) {
    const double inv = 1.0 / std::sqrt(2.0);
    return {{{inv, -std::exp(std::complex<double>(0.0, lambda)) * inv},
             {std::exp(std::complex<double>(0.0, phi)) * inv,
              std::exp(std::complex<double>(0.0, phi + lambda)) * inv}}};
}

Mat2 U3(double theta, double phi, double lambda) {
    double c = std::cos(theta / 2.0);
    double s = std::sin(theta / 2.0);
    return {{{c, -std::exp(std::complex<double>(0.0, lambda)) * s},
             {std::exp(std::complex<double>(0.0, phi)) * s,
              std::exp(std::complex<double>(0.0, phi + lambda)) * c}}};
}

} // namespace gates

Edge Package::gate(const Mat2& u, std::uint16_t target, std::span<const Control> ctrls) {
    if (nqubits_ == 0) return Edge{kTerminal, kC1};
    assert(target < nqubits_);

    auto find_ctrl = [&](std::uint16_t q) -> const Control* {
        for (const auto& c : ctrls) {
            if (c.qubit == q) return &c;
        }
        return nullptr;
    };

    Edge zero_edge{kTerminal, kC0};

    // Sub-DDs below the target qubit
    std::array<std::array<Edge, 2>, 2> F;
    for (std::size_t i = 0; i < 2; ++i) {
        for (std::size_t j = 0; j < 2; ++j) {
            F[i][j] = Edge{kTerminal, complex_table_.intern(u[i][j])};
        }
    }

    for (std::uint16_t q = 0; q < target; ++q) {
        const Control* c = find_ctrl(q);
        Edge id_below = (q == 0) ? Edge{kTerminal, kC1} : identity(q - 1);

        for (std::size_t i = 0; i < 2; ++i) {
            for (std::size_t j = 0; j < 2; ++j) {
                if (!c) {
                    F[i][j] = make_mnode(q, {F[i][j], zero_edge, zero_edge, F[i][j]});
                } else {
                    Edge inact = (i == j) ? id_below : zero_edge;
                    Edge act = F[i][j];
                    if (c->positive) {
                        F[i][j] = make_mnode(q, {inact, zero_edge, zero_edge, act});
                    } else {
                        F[i][j] = make_mnode(q, {act, zero_edge, zero_edge, inact});
                    }
                }
            }
        }
    }

    // At the target level
    Edge curr = make_mnode(target, {F[0][0], F[0][1], F[1][0], F[1][1]});

    // Levels above target
    for (std::uint16_t q = target + 1; q < static_cast<std::uint16_t>(nqubits_); ++q) {
        const Control* c = find_ctrl(q);
        if (!c) {
            curr = make_mnode(q, {curr, zero_edge, zero_edge, curr});
        } else {
            Edge id_below = identity(q - 1);
            if (c->positive) {
                curr = make_mnode(q, {id_below, zero_edge, zero_edge, curr});
            } else {
                curr = make_mnode(q, {curr, zero_edge, zero_edge, id_below});
            }
        }
    }

    return curr;
}

} // namespace qdd

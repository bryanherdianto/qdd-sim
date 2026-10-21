#include "qdd/dd.hpp"

namespace qdd {

Edge Package::adjoint(Edge m) {
    if (m.is_zero()) return Edge{kTerminal, kC0};
    if (m.is_terminal()) {
        return Edge{kTerminal, complex_table_.conj(m.w)};
    }

    // Fast path: identity is self-adjoint
    if (!m.is_terminal() && m_unique_.get(m.raw_node()).identity) {
        return Edge{m.node, complex_table_.conj(m.w)};
    }

    // Weight-free cache lookup on m.raw_node
    std::uint64_t h = (static_cast<std::uint64_t>(m.raw_node()) | 0x80000000ULL) * 0x9e3779b97f4a7c15ULL;
    std::size_t idx = static_cast<std::size_t>(h) & cache_mask_;

    stats_.adjoint_lookups++;
    const auto& entry = adjoint_cache_[idx];
    if (entry.m_node == (m.raw_node() | kMatrixFlag)) {
        stats_.adjoint_hits++;
        CIdx total_w = complex_table_.mul(complex_table_.conj(m.w), entry.result.w);
        return Edge{entry.result.node, total_w};
    }

    Edge norm_res = adjoint_rec(Edge{m.raw_node() | kMatrixFlag, kC1});
    adjoint_cache_[idx] = AdjointCacheEntry{m.raw_node() | kMatrixFlag, norm_res};

    CIdx total_w = complex_table_.mul(complex_table_.conj(m.w), norm_res.w);
    return Edge{norm_res.node, total_w};
}

Edge Package::adjoint_rec(Edge m) {
    if (m.is_zero()) return Edge{kTerminal, kC0};
    if (m.is_terminal()) {
        return Edge{kTerminal, complex_table_.conj(m.w)};
    }

    if (!m.is_terminal() && m_unique_.get(m.raw_node()).identity) {
        return Edge{m.node, complex_table_.conj(m.w)};
    }

    const auto& mn = m_unique_.get(m.raw_node());
    std::uint16_t q = mn.qubit;

    Edge e00 = Edge{mn.e[0].node, complex_table_.mul(m.w, mn.e[0].w)};
    Edge e01 = Edge{mn.e[1].node, complex_table_.mul(m.w, mn.e[1].w)};
    Edge e10 = Edge{mn.e[2].node, complex_table_.mul(m.w, mn.e[2].w)};
    Edge e11 = Edge{mn.e[3].node, complex_table_.mul(m.w, mn.e[3].w)};

    // Note block swap: e01 and e10 are transposed
    Edge r00 = adjoint(e00);
    Edge r01 = adjoint(e10);
    Edge r10 = adjoint(e01);
    Edge r11 = adjoint(e11);

    return make_mnode(q, {r00, r01, r10, r11});
}

} // namespace qdd

#include "qdd/dd.hpp"

namespace qdd {

Edge Package::mat_vec(Edge m, Edge v) {
    if (m.is_zero() || v.is_zero()) return Edge{kTerminal, kC0};
    if (m.is_terminal() && v.is_terminal()) {
        return Edge{kTerminal, complex_table_.mul(m.w, v.w)};
    }

    // Fast-path for identity matrix
    if (!m.is_terminal() && m_unique_.get(m.raw_node()).identity) {
        return Edge{v.node, complex_table_.mul(m.w, v.w)};
    }

    // Weight-free cache lookup on (m.raw_node, v.raw_node)
    std::uint64_t h = (static_cast<std::uint64_t>(m.raw_node()) | 0x80000000ULL) * 0x9e3779b97f4a7c15ULL;
    h ^= static_cast<std::uint64_t>(v.raw_node()) * 0xbf58476d1ce4e5b9ULL;
    std::size_t idx = static_cast<std::size_t>(h) & cache_mask_;

    stats_.mat_vec_lookups++;
    const auto& entry = mat_vec_cache_[idx];
    if (entry.m_node == (m.raw_node() | kMatrixFlag) && entry.v_node == v.raw_node()) {
        stats_.mat_vec_hits++;
        CIdx total_w = complex_table_.mul(complex_table_.mul(m.w, v.w), entry.result.w);
        return Edge{entry.result.node, total_w};
    }

    Edge norm_res = mat_vec_rec(Edge{m.raw_node() | kMatrixFlag, kC1}, Edge{v.raw_node(), kC1});
    mat_vec_cache_[idx] = MatVecCacheEntry{m.raw_node() | kMatrixFlag, v.raw_node(), norm_res};

    CIdx total_w = complex_table_.mul(complex_table_.mul(m.w, v.w), norm_res.w);
    return Edge{norm_res.node, total_w};
}

Edge Package::mat_vec_rec(Edge m, Edge v) {
    if (m.is_zero() || v.is_zero()) return Edge{kTerminal, kC0};
    if (m.is_terminal() && v.is_terminal()) {
        return Edge{kTerminal, complex_table_.mul(m.w, v.w)};
    }

    if (!m.is_terminal() && m_unique_.get(m.raw_node()).identity) {
        return Edge{v.node, complex_table_.mul(m.w, v.w)};
    }

    std::uint16_t qm = (!m.is_terminal()) ? m_unique_.get(m.raw_node()).qubit : 0;
    std::uint16_t qv = (!v.is_terminal()) ? v_unique_.get(v.raw_node()).qubit : 0;
    std::uint16_t q = std::max(qm, qv);

    Edge m00{}, m01{}, m10{}, m11{};
    if (!m.is_terminal() && qm == q) {
        const auto& mn = m_unique_.get(m.raw_node());
        m00 = Edge{mn.e[0].node, complex_table_.mul(m.w, mn.e[0].w)};
        m01 = Edge{mn.e[1].node, complex_table_.mul(m.w, mn.e[1].w)};
        m10 = Edge{mn.e[2].node, complex_table_.mul(m.w, mn.e[2].w)};
        m11 = Edge{mn.e[3].node, complex_table_.mul(m.w, mn.e[3].w)};
    } else {
        m00 = m;
        m01 = Edge{kTerminal, kC0};
        m10 = Edge{kTerminal, kC0};
        m11 = m;
    }

    Edge v0{}, v1{};
    if (!v.is_terminal() && qv == q) {
        const auto& vn = v_unique_.get(v.raw_node());
        v0 = Edge{vn.e[0].node, complex_table_.mul(v.w, vn.e[0].w)};
        v1 = Edge{vn.e[1].node, complex_table_.mul(v.w, vn.e[1].w)};
    } else {
        v0 = v;
        v1 = Edge{kTerminal, kC0};
    }

    Edge p00 = mat_vec(m00, v0);
    Edge p01 = mat_vec(m01, v1);
    Edge p10 = mat_vec(m10, v0);
    Edge p11 = mat_vec(m11, v1);

    Edge r0 = add_v(p00, p01);
    Edge r1 = add_v(p10, p11);

    return make_vnode(q, {r0, r1});
}

Edge Package::mat_mat(Edge a, Edge b) {
    if (a.is_zero() || b.is_zero()) return Edge{kTerminal, kC0};
    if (a.is_terminal() && b.is_terminal()) {
        return Edge{kTerminal, complex_table_.mul(a.w, b.w)};
    }

    // Fast-path for identity
    if (!a.is_terminal() && m_unique_.get(a.raw_node()).identity) {
        return Edge{b.node, complex_table_.mul(a.w, b.w)};
    }
    if (!b.is_terminal() && m_unique_.get(b.raw_node()).identity) {
        return Edge{a.node, complex_table_.mul(a.w, b.w)};
    }

    // Weight-free cache lookup on (a.raw_node, b.raw_node)
    std::uint64_t h = (static_cast<std::uint64_t>(a.raw_node()) | 0x80000000ULL) * 0x9e3779b97f4a7c15ULL;
    h ^= (static_cast<std::uint64_t>(b.raw_node()) | 0x80000000ULL) * 0xbf58476d1ce4e5b9ULL;
    std::size_t idx = static_cast<std::size_t>(h) & cache_mask_;

    stats_.mat_mat_lookups++;
    const auto& entry = mat_mat_cache_[idx];
    if (entry.a_node == (a.raw_node() | kMatrixFlag) && entry.b_node == (b.raw_node() | kMatrixFlag)) {
        stats_.mat_mat_hits++;
        CIdx total_w = complex_table_.mul(complex_table_.mul(a.w, b.w), entry.result.w);
        return Edge{entry.result.node, total_w};
    }

    Edge norm_res = mat_mat_rec(Edge{a.raw_node() | kMatrixFlag, kC1}, Edge{b.raw_node() | kMatrixFlag, kC1});
    mat_mat_cache_[idx] = MatMatCacheEntry{a.raw_node() | kMatrixFlag, b.raw_node() | kMatrixFlag, norm_res};

    CIdx total_w = complex_table_.mul(complex_table_.mul(a.w, b.w), norm_res.w);
    return Edge{norm_res.node, total_w};
}

Edge Package::mat_mat_rec(Edge a, Edge b) {
    if (a.is_zero() || b.is_zero()) return Edge{kTerminal, kC0};
    if (a.is_terminal() && b.is_terminal()) {
        return Edge{kTerminal, complex_table_.mul(a.w, b.w)};
    }

    if (!a.is_terminal() && m_unique_.get(a.raw_node()).identity) {
        return Edge{b.node, complex_table_.mul(a.w, b.w)};
    }
    if (!b.is_terminal() && m_unique_.get(b.raw_node()).identity) {
        return Edge{a.node, complex_table_.mul(a.w, b.w)};
    }

    std::uint16_t qa = (!a.is_terminal()) ? m_unique_.get(a.raw_node()).qubit : 0;
    std::uint16_t qb = (!b.is_terminal()) ? m_unique_.get(b.raw_node()).qubit : 0;
    std::uint16_t q = std::max(qa, qb);

    Edge a00{}, a01{}, a10{}, a11{};
    if (!a.is_terminal() && qa == q) {
        const auto& mn = m_unique_.get(a.raw_node());
        a00 = Edge{mn.e[0].node, complex_table_.mul(a.w, mn.e[0].w)};
        a01 = Edge{mn.e[1].node, complex_table_.mul(a.w, mn.e[1].w)};
        a10 = Edge{mn.e[2].node, complex_table_.mul(a.w, mn.e[2].w)};
        a11 = Edge{mn.e[3].node, complex_table_.mul(a.w, mn.e[3].w)};
    } else {
        a00 = a; a01 = Edge{kTerminal, kC0}; a10 = Edge{kTerminal, kC0}; a11 = a;
    }

    Edge b00{}, b01{}, b10{}, b11{};
    if (!b.is_terminal() && qb == q) {
        const auto& mn = m_unique_.get(b.raw_node());
        b00 = Edge{mn.e[0].node, complex_table_.mul(b.w, mn.e[0].w)};
        b01 = Edge{mn.e[1].node, complex_table_.mul(b.w, mn.e[1].w)};
        b10 = Edge{mn.e[2].node, complex_table_.mul(b.w, mn.e[2].w)};
        b11 = Edge{mn.e[3].node, complex_table_.mul(b.w, mn.e[3].w)};
    } else {
        b00 = b; b01 = Edge{kTerminal, kC0}; b10 = Edge{kTerminal, kC0}; b11 = b;
    }

    Edge r00 = add_m(mat_mat(a00, b00), mat_mat(a01, b10));
    Edge r01 = add_m(mat_mat(a00, b01), mat_mat(a01, b11));
    Edge r10 = add_m(mat_mat(a10, b00), mat_mat(a11, b10));
    Edge r11 = add_m(mat_mat(a10, b01), mat_mat(a11, b11));

    return make_mnode(q, {r00, r01, r10, r11});
}

} // namespace qdd

#include "qdd/dd.hpp"

namespace qdd {

Edge Package::add(Edge a, Edge b) {
    if (a.is_matrix() || b.is_matrix()) {
        return add_m(a, b);
    }
    return add_v(a, b);
}

// ============================================================================
// Vector Addition
// ============================================================================

Edge Package::add_v(Edge a, Edge b) {
    if (a.is_zero()) return b;
    if (b.is_zero()) return a;
    return add_v_rec(a, b);
}

Edge Package::add_v_rec(Edge a, Edge b) {
    if (a.is_zero()) return b;
    if (b.is_zero()) return a;

    if (a.raw_node() == b.raw_node()) {
        CIdx sum_w = complex_table_.add(a.w, b.w);
        if (sum_w == kC0) return Edge{kTerminal, kC0};
        return Edge{a.raw_node(), sum_w};
    }

    if (a.is_terminal() && b.is_terminal()) {
        CIdx sum_w = complex_table_.add(a.w, b.w);
        return Edge{kTerminal, sum_w};
    }

    // Canonical order for commutativity
    if (a.raw_node() > b.raw_node() || (a.raw_node() == b.raw_node() && a.w > b.w)) {
        std::swap(a, b);
    }

    std::uint64_t h = static_cast<std::uint64_t>(a.raw_node()) * 0x9e3779b97f4a7c15ULL;
    h ^= static_cast<std::uint64_t>(a.w) * 0xbf58476d1ce4e5b9ULL;
    h ^= static_cast<std::uint64_t>(b.raw_node()) * 0x94d049bb133111ebULL;
    h ^= static_cast<std::uint64_t>(b.w) * 0x85b061e52c9261feULL;
    std::size_t idx = static_cast<std::size_t>(h) & cache_mask_;

    stats_.add_lookups++;
    const auto& entry = add_cache_[idx];
    if (entry.a_node == a.raw_node() && entry.a_w == a.w &&
        entry.b_node == b.raw_node() && entry.b_w == b.w) {
        stats_.add_hits++;
        return entry.result;
    }

    std::uint16_t qa = (!a.is_terminal()) ? v_unique_.get(a.raw_node()).qubit : 0;
    std::uint16_t qb = (!b.is_terminal()) ? v_unique_.get(b.raw_node()).qubit : 0;
    std::uint16_t q = std::max(qa, qb);

    std::array<Edge, 2> child_a{}, child_b{};
    if (!a.is_terminal() && qa == q) {
        const auto& vn = v_unique_.get(a.raw_node());
        child_a[0] = Edge{vn.e[0].node, complex_table_.mul(a.w, vn.e[0].w)};
        child_a[1] = Edge{vn.e[1].node, complex_table_.mul(a.w, vn.e[1].w)};
    } else {
        child_a[0] = a;
        child_a[1] = Edge{kTerminal, kC0};
    }

    if (!b.is_terminal() && qb == q) {
        const auto& vn = v_unique_.get(b.raw_node());
        child_b[0] = Edge{vn.e[0].node, complex_table_.mul(b.w, vn.e[0].w)};
        child_b[1] = Edge{vn.e[1].node, complex_table_.mul(b.w, vn.e[1].w)};
    } else {
        child_b[0] = b;
        child_b[1] = Edge{kTerminal, kC0};
    }

    Edge r0 = add_v_rec(child_a[0], child_b[0]);
    Edge r1 = add_v_rec(child_a[1], child_b[1]);
    Edge res = make_vnode(q, {r0, r1});

    add_cache_[idx] = AddCacheEntry{a.raw_node(), a.w, b.raw_node(), b.w, res};
    return res;
}

// ============================================================================
// Matrix Addition
// ============================================================================

Edge Package::add_m(Edge a, Edge b) {
    if (a.is_zero()) return b;
    if (b.is_zero()) return a;
    return add_m_rec(a, b);
}

Edge Package::add_m_rec(Edge a, Edge b) {
    if (a.is_zero()) return b;
    if (b.is_zero()) return a;

    if (a.raw_node() == b.raw_node()) {
        CIdx sum_w = complex_table_.add(a.w, b.w);
        if (sum_w == kC0) return Edge{kTerminal, kC0};
        if (a.is_terminal()) return Edge{kTerminal, sum_w};
        return Edge{a.raw_node() | kMatrixFlag, sum_w};
    }

    if (a.is_terminal() && b.is_terminal()) {
        CIdx sum_w = complex_table_.add(a.w, b.w);
        return Edge{kTerminal, sum_w};
    }

    // Canonical order for commutativity
    if (a.raw_node() > b.raw_node() || (a.raw_node() == b.raw_node() && a.w > b.w)) {
        std::swap(a, b);
    }

    // Hash with bit 31 set so matrix and vector cache entries never collide
    std::uint64_t h = (static_cast<std::uint64_t>(a.raw_node()) | 0x80000000ULL) * 0x9e3779b97f4a7c15ULL;
    h ^= static_cast<std::uint64_t>(a.w) * 0xbf58476d1ce4e5b9ULL;
    h ^= (static_cast<std::uint64_t>(b.raw_node()) | 0x80000000ULL) * 0x94d049bb133111ebULL;
    h ^= static_cast<std::uint64_t>(b.w) * 0x85b061e52c9261feULL;
    std::size_t idx = static_cast<std::size_t>(h) & cache_mask_;

    stats_.add_lookups++;
    const auto& entry = add_cache_[idx];
    if (entry.a_node == (a.raw_node() | kMatrixFlag) && entry.a_w == a.w &&
        entry.b_node == (b.raw_node() | kMatrixFlag) && entry.b_w == b.w) {
        stats_.add_hits++;
        return entry.result;
    }

    std::uint16_t qa = (!a.is_terminal()) ? m_unique_.get(a.raw_node()).qubit : 0;
    std::uint16_t qb = (!b.is_terminal()) ? m_unique_.get(b.raw_node()).qubit : 0;
    std::uint16_t q = std::max(qa, qb);

    std::array<Edge, 4> child_a{}, child_b{};
    if (!a.is_terminal() && qa == q) {
        const auto& mn = m_unique_.get(a.raw_node());
        for (std::size_t i = 0; i < 4; ++i) {
            child_a[i] = Edge{mn.e[i].node, complex_table_.mul(a.w, mn.e[i].w)};
        }
    } else {
        child_a[0] = a;
        child_a[1] = Edge{kTerminal, kC0};
        child_a[2] = Edge{kTerminal, kC0};
        child_a[3] = a;
    }

    if (!b.is_terminal() && qb == q) {
        const auto& mn = m_unique_.get(b.raw_node());
        for (std::size_t i = 0; i < 4; ++i) {
            child_b[i] = Edge{mn.e[i].node, complex_table_.mul(b.w, mn.e[i].w)};
        }
    } else {
        child_b[0] = b;
        child_b[1] = Edge{kTerminal, kC0};
        child_b[2] = Edge{kTerminal, kC0};
        child_b[3] = b;
    }

    std::array<Edge, 4> r{};
    for (std::size_t i = 0; i < 4; ++i) {
        r[i] = add_m_rec(child_a[i], child_b[i]);
    }
    Edge res = make_mnode(q, r);

    add_cache_[idx] = AddCacheEntry{a.raw_node() | kMatrixFlag, a.w,
                                    b.raw_node() | kMatrixFlag, b.w, res};
    return res;
}

} // namespace qdd

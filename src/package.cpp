#include "qdd/dd.hpp"

#include <algorithm>
#include <cassert>
#include <random>
#include <stdexcept>

namespace qdd {

// ============================================================================
// UniqueTable implementation
// ============================================================================

template <std::size_t Arity>
UniqueTable<Arity>::UniqueTable(std::size_t initial_capacity) {
    std::size_t cap = 1;
    while (cap < initial_capacity) cap <<= 1;

    buckets_.assign(cap, 0);
    nodes_.reserve(cap);
    // Node 0 is the terminal node
    NodeT<Arity> term{};
    nodes_.push_back(term);
}

template <std::size_t Arity>
std::size_t UniqueTable<Arity>::hash_node(const NodeT<Arity>& node) {
    std::uint64_t h = static_cast<std::uint64_t>(node.qubit);
    for (std::size_t i = 0; i < Arity; ++i) {
        h = (h ^ static_cast<std::uint64_t>(node.e[i].node)) * 0xbf58476d1ce4e5b9ULL;
        h = (h ^ static_cast<std::uint64_t>(node.e[i].w)) * 0x94d049bb133111ebULL;
    }
    h = (h ^ (h >> 30)) * 0xbf58476d1ce4e5b9ULL;
    h = (h ^ (h >> 27)) * 0x94d049bb133111ebULL;
    return static_cast<std::size_t>(h ^ (h >> 31));
}

template <std::size_t Arity>
NodeID UniqueTable<Arity>::lookup_or_insert(const NodeT<Arity>& candidate) {
    std::size_t mask = buckets_.size() - 1;
    std::size_t b = hash_node(candidate) & mask;

    NodeID curr = buckets_[b];
    while (curr != 0) {
        if (nodes_[curr] == candidate) {
            return curr;
        }
        curr = nodes_[curr].next;
    }

    // Insert new node
    NodeID new_id = 0;
    if (free_head_ != 0) {
        new_id = free_head_;
        free_head_ = nodes_[new_id].next;
        nodes_[new_id] = candidate;
    } else {
        new_id = static_cast<NodeID>(nodes_.size());
        nodes_.push_back(candidate);
    }

    nodes_[new_id].next = buckets_[b];
    buckets_[b] = new_id;

    active_count_++;
    peak_count_ = std::max(peak_count_, active_count_);

    if (active_count_ >= buckets_.size()) {
        grow();
    }

    return new_id;
}

template <std::size_t Arity>
void UniqueTable<Arity>::grow() {
    std::size_t new_cap = buckets_.size() * 2;
    std::vector<NodeID> old_buckets = std::move(buckets_);
    buckets_.assign(new_cap, 0);
    std::size_t mask = new_cap - 1;

    for (NodeID head : old_buckets) {
        NodeID curr = head;
        while (curr != 0) {
            NodeID nxt = nodes_[curr].next;
            std::size_t b = hash_node(nodes_[curr]) & mask;
            nodes_[curr].next = buckets_[b];
            buckets_[b] = curr;
            curr = nxt;
        }
    }
}

template <std::size_t Arity>
void UniqueTable<Arity>::clear() {
    buckets_.assign(buckets_.size(), 0);
    nodes_.resize(1); // Keep terminal 0
    free_head_ = 0;
    active_count_ = 0;
}

template <std::size_t Arity>
void UniqueTable<Arity>::sweep() {
    buckets_.assign(buckets_.size(), 0);
    std::size_t mask = buckets_.size() - 1;
    free_head_ = 0;
    active_count_ = 0;

    for (NodeID id = 1; id < static_cast<NodeID>(nodes_.size()); ++id) {
        if (nodes_[id].mark) {
            nodes_[id].mark = false; // Reset mark for next GC
            std::size_t b = hash_node(nodes_[id]) & mask;
            nodes_[id].next = buckets_[b];
            buckets_[b] = id;
            active_count_++;
        } else {
            // Node is unreferenced: recycle
            nodes_[id].qubit = 0;
            nodes_[id].identity = false;
            for (std::size_t i = 0; i < Arity; ++i) {
                nodes_[id].e[i] = {};
            }
            nodes_[id].next = free_head_;
            free_head_ = id;
        }
    }
}

// Explicit template instantiations
template class UniqueTable<2>;
template class UniqueTable<4>;

// ============================================================================
// Package::Root implementation
// ============================================================================

Package::Root::Root(Package& pkg, Edge edge)
    : pkg_(&pkg), edge_(edge) {
    pkg_->register_root(&edge_);
}

Package::Root::~Root() {
    if (pkg_) {
        pkg_->unregister_root(&edge_);
    }
}

Package::Root::Root(Root&& other) noexcept
    : pkg_(other.pkg_), edge_(other.edge_) {
    if (pkg_) {
        pkg_->unregister_root(&other.edge_);
        pkg_->register_root(&edge_);
    }
    other.pkg_ = nullptr;
}

Package::Root& Package::Root::operator=(Root&& other) noexcept {
    if (this != &other) {
        if (pkg_) {
            pkg_->unregister_root(&edge_);
        }
        pkg_ = other.pkg_;
        edge_ = other.edge_;
        if (pkg_) {
            pkg_->unregister_root(&other.edge_);
            pkg_->register_root(&edge_);
        }
        other.pkg_ = nullptr;
    }
    return *this;
}

// ============================================================================
// Package implementation
// ============================================================================

Package::Package(std::size_t nqubits, PackageConfig cfg)
    : nqubits_(nqubits),
      cfg_(cfg),
      complex_table_(cfg.tol),
      v_unique_(cfg.initial_nodes),
      m_unique_(cfg.initial_nodes),
      next_gc_threshold_(cfg.gc_threshold) {
    std::size_t cache_size = 1ULL << cfg.cache_log2;
    cache_mask_ = cache_size - 1;

    add_cache_.assign(cache_size, {});
    mat_vec_cache_.assign(cache_size, {});
    mat_mat_cache_.assign(cache_size, {});
    adjoint_cache_.assign(cache_size, {});

    init_identities();
}

void Package::init_identities() {
    identities_.assign(nqubits_, 0);
    if (nqubits_ == 0) return;

    // Level 0 identity
    std::array<Edge, 4> e0{
        Edge{kTerminal, kC1},
        Edge{kTerminal, kC0},
        Edge{kTerminal, kC0},
        Edge{kTerminal, kC1}
    };
    Edge id0 = make_mnode(0, e0);
    identities_[0] = id0.node;

    // Levels 1 .. nqubits-1
    for (std::uint16_t q = 1; q < static_cast<std::uint16_t>(nqubits_); ++q) {
        std::array<Edge, 4> eq{
            Edge{identities_[q - 1], kC1},
            Edge{kTerminal, kC0},
            Edge{kTerminal, kC0},
            Edge{identities_[q - 1], kC1}
        };
        Edge id_q = make_mnode(q, eq);
        identities_[q] = id_q.node;
    }
}

Edge Package::identity() {
    if (nqubits_ == 0) return Edge{kTerminal, kC1};
    return Edge{identities_[nqubits_ - 1], kC1};
}

Edge Package::identity(std::uint16_t q) {
    if (q >= nqubits_) return identity();
    return Edge{identities_[q], kC1};
}

Edge Package::make_vnode(std::uint16_t q, std::array<Edge, 2> children) {
    if (children[0].is_zero() && children[1].is_zero()) {
        return Edge{kTerminal, kC0};
    }

    double mag0 = complex_table_.mag(children[0].w);
    double mag1 = complex_table_.mag(children[1].w);

    CIdx factor = kC0;
    if (mag0 >= mag1) {
        factor = children[0].w;
        children[0].w = kC1;
        children[1].w = complex_table_.div(children[1].w, factor);
    } else {
        factor = children[1].w;
        children[0].w = complex_table_.div(children[0].w, factor);
        children[1].w = kC1;
    }

    VNode cand;
    cand.qubit = q;
    cand.e = children;

    NodeID id = v_unique_.lookup_or_insert(cand);
    return Edge{id, factor};
}

Edge Package::make_mnode(std::uint16_t q, std::array<Edge, 4> children) {
    bool all_zero = true;
    for (std::size_t i = 0; i < 4; ++i) {
        if (!children[i].is_zero()) {
            all_zero = false;
            break;
        }
    }
    if (all_zero) {
        return Edge{kTerminal, kC0};
    }

    std::size_t best_i = 0;
    double max_mag = -1.0;
    for (std::size_t i = 0; i < 4; ++i) {
        double m = complex_table_.mag(children[i].w);
        if (m > max_mag + complex_table_.tolerance()) {
            max_mag = m;
            best_i = i;
        }
    }

    CIdx factor = children[best_i].w;
    for (std::size_t i = 0; i < 4; ++i) {
        if (i == best_i) {
            children[i].w = kC1;
        } else {
            children[i].w = complex_table_.div(children[i].w, factor);
        }
    }

    // Check identity property
    bool is_id = false;
    if (children[1].is_zero() && children[2].is_zero() &&
        children[0].w == kC1 && children[3].w == kC1) {
        if (q == 0) {
            is_id = (children[0].is_terminal() && children[3].is_terminal());
        } else {
            is_id = (children[0].raw_node() == children[3].raw_node() &&
                     m_unique_.get(children[0].raw_node()).identity);
        }
    }

    MNode cand;
    cand.qubit = q;
    cand.e = children;
    cand.identity = is_id;

    NodeID id = m_unique_.lookup_or_insert(cand);
    return Edge{id | kMatrixFlag, factor};
}

Edge Package::zero_state() {
    if (nqubits_ == 0) return Edge{kTerminal, kC1};
    Edge curr{kTerminal, kC1};
    for (std::uint16_t q = 0; q < static_cast<std::uint16_t>(nqubits_); ++q) {
        curr = make_vnode(q, {curr, Edge{kTerminal, kC0}});
    }
    return curr;
}

Edge Package::basis_state(std::uint64_t val) {
    if (nqubits_ == 0) return Edge{kTerminal, kC1};
    Edge curr{kTerminal, kC1};
    for (std::uint16_t q = 0; q < static_cast<std::uint16_t>(nqubits_); ++q) {
        bool bit = (val >> q) & 1ULL;
        if (!bit) {
            curr = make_vnode(q, {curr, Edge{kTerminal, kC0}});
        } else {
            curr = make_vnode(q, {Edge{kTerminal, kC0}, curr});
        }
    }
    return curr;
}

std::complex<double> Package::amplitude(Edge v, std::uint64_t basis) const {
    if (v.is_zero()) return {0.0, 0.0};
    if (nqubits_ == 0) return complex_table_.value(v.w);

    CIdx curr_w = v.w;
    NodeID curr_id = v.node;

    for (int q = static_cast<int>(nqubits_) - 1; q >= 0; --q) {
        if (curr_id == kTerminal) {
            if (curr_w == kC0) return {0.0, 0.0};
            break;
        }
        const auto& node = v_unique_.get(curr_id);
        if (node.qubit == static_cast<std::uint16_t>(q)) {
            int bit = static_cast<int>((basis >> q) & 1ULL);
            const auto& child = node.e[static_cast<std::size_t>(bit)];
            if (child.is_zero()) return {0.0, 0.0};
            curr_w = const_cast<ComplexTable&>(complex_table_).mul(curr_w, child.w);
            curr_id = child.node;
        } else {
            // Skipped variable: shouldn't happen with standard structure
            return {0.0, 0.0};
        }
    }
    return complex_table_.value(curr_w);
}

std::vector<std::complex<double>> Package::get_all_amplitudes(Edge v) const {
    if (nqubits_ > 20) {
        throw std::runtime_error("State vector too large for get_all_amplitudes (> 20 qubits)");
    }
    std::size_t dim = 1ULL << nqubits_;
    std::vector<std::complex<double>> res(dim, {0.0, 0.0});
    for (std::size_t i = 0; i < dim; ++i) {
        res[i] = amplitude(v, static_cast<std::uint64_t>(i));
    }
    return res;
}

std::string Package::sample(Edge v, std::uint64_t seed) {
    if (v.is_zero()) return std::string(nqubits_, '0');
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    std::string outcome(nqubits_, '0');
    NodeID curr_id = v.node;

    for (int q = static_cast<int>(nqubits_) - 1; q >= 0; --q) {
        if (curr_id == kTerminal) break;
        const auto& node = v_unique_.get(curr_id);
        assert(node.qubit == static_cast<std::uint16_t>(q));

        double p0 = 0.0;
        if (!node.e[0].is_zero()) {
            p0 = complex_table_.mag_sq(node.e[0].w);
        }
        double p1 = 0.0;
        if (!node.e[1].is_zero()) {
            p1 = complex_table_.mag_sq(node.e[1].w);
        }
        double total = p0 + p1;
        if (total <= 1e-15) break;

        double prob_zero = p0 / total;
        double r = dist(rng);
        std::size_t out_idx = nqubits_ - 1 - static_cast<std::size_t>(q);
        if (r < prob_zero) {
            outcome[out_idx] = '0';
            curr_id = node.e[0].node;
        } else {
            outcome[out_idx] = '1';
            curr_id = node.e[1].node;
        }
    }
    return outcome;
}

// ============================================================================
// Root management & Garbage Collection
// ============================================================================

void Package::register_root(Edge* root) {
    roots_.insert(root);
}

void Package::unregister_root(Edge* root) {
    roots_.erase(root);
}

void Package::clear_caches() {
    for (auto& entry : add_cache_) entry = {};
    for (auto& entry : mat_vec_cache_) entry = {};
    for (auto& entry : mat_mat_cache_) entry = {};
    for (auto& entry : adjoint_cache_) entry = {};
}

void Package::mark_vnode_rec(NodeID id) {
    if (id == kTerminal) return;
    auto& node = v_unique_.get_mut(id);
    if (node.mark) return;
    node.mark = true;
    for (std::size_t i = 0; i < 2; ++i) {
        if (!node.e[i].is_zero()) {
            mark_vnode_rec(node.e[i].node);
        }
    }
}

void Package::mark_mnode_rec(NodeID id) {
    id = id & ~kMatrixFlag;
    if (id == kTerminal) return;
    auto& node = m_unique_.get_mut(id);
    if (node.mark) return;
    node.mark = true;
    for (std::size_t i = 0; i < 4; ++i) {
        if (!node.e[i].is_zero()) {
            mark_mnode_rec(node.e[i].raw_node());
        }
    }
}

void Package::gc() {
    // Mark roots
    for (Edge* r : roots_) {
        if (r && !r->is_zero()) {
            if (r->is_matrix()) {
                mark_mnode_rec(r->raw_node());
            } else {
                mark_vnode_rec(r->raw_node());
            }
        }
    }

    // Protect all identities
    for (NodeID id_node : identities_) {
        mark_mnode_rec(id_node & ~kMatrixFlag);
    }

    v_unique_.sweep();
    m_unique_.sweep();
    clear_caches();

    stats_.gc_count++;
    next_gc_threshold_ = std::max(cfg_.gc_threshold, (v_unique_.size() + m_unique_.size()) * 2);
}

void Package::check_gc() {
    std::size_t total_nodes = v_unique_.size() + m_unique_.size();
    if (cfg_.max_nodes > 0 && total_nodes >= cfg_.max_nodes) {
        gc();
        total_nodes = v_unique_.size() + m_unique_.size();
        if (total_nodes >= cfg_.max_nodes) {
            throw std::runtime_error("Node limit exceeded");
        }
    } else if (total_nodes >= next_gc_threshold_) {
        gc();
    }
}

Stats Package::stats() const {
    stats_.vector_nodes = v_unique_.size();
    stats_.matrix_nodes = m_unique_.size();
    stats_.peak_vector_nodes = v_unique_.peak_size();
    stats_.peak_matrix_nodes = m_unique_.peak_size();
    stats_.complex_entries = complex_table_.size();
    return stats_;
}

void Package::reset_stats() {
    stats_ = Stats{};
}

} // namespace qdd

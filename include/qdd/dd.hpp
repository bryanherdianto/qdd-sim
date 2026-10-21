#pragma once

#include "qdd/complex_table.hpp"
#include "qdd/stats.hpp"

#include <array>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <span>
#include <string>
#include <vector>
#include <unordered_set>

namespace qdd {

using NodeID = std::uint32_t;
inline constexpr NodeID kTerminal = 0;
inline constexpr NodeID kMatrixFlag = 1U << 31;

struct Edge {
    NodeID node = kTerminal;
    CIdx w = kC0;

    bool is_zero() const { return w == kC0; }
    bool is_terminal() const { return (node & ~kMatrixFlag) == kTerminal; }
    bool is_matrix() const { return (node & kMatrixFlag) != 0; }
    NodeID raw_node() const { return node & ~kMatrixFlag; }
    bool operator==(const Edge& other) const = default;
};

template <std::size_t Arity>
struct NodeT {
    std::uint16_t qubit = 0;
    std::array<Edge, Arity> e{};
    bool identity = false;
    bool mark = false;
    NodeID next = 0;

    bool operator==(const NodeT<Arity>& other) const {
        if (qubit != other.qubit) return false;
        for (std::size_t i = 0; i < Arity; ++i) {
            if (e[i] != other.e[i]) return false;
        }
        return true;
    }
};

using VNode = NodeT<2>;
using MNode = NodeT<4>;

struct Control {
    std::uint16_t qubit;
    bool positive = true;
};

using Mat2 = std::array<std::array<std::complex<double>, 2>, 2>;

struct PackageConfig {
    std::size_t initial_nodes = 4096;
    std::size_t max_nodes = 0;
    std::size_t gc_threshold = 100000;
    std::size_t cache_log2 = 18;
    double tol = ComplexTable::kDefaultTol;
};

template <std::size_t Arity>
class UniqueTable {
public:
    explicit UniqueTable(std::size_t initial_capacity = 4096);

    NodeID lookup_or_insert(const NodeT<Arity>& candidate);
    const NodeT<Arity>& get(NodeID id) const { return nodes_[id]; }
    NodeT<Arity>& get_mut(NodeID id) { return nodes_[id]; }

    std::size_t size() const { return active_count_; }
    std::size_t peak_size() const { return peak_count_; }
    std::size_t capacity() const { return nodes_.size(); }

    void mark(NodeID id);
    void sweep();
    void clear();

private:
    std::vector<NodeT<Arity>> nodes_;
    std::vector<NodeID> buckets_;
    NodeID free_head_ = 0;
    std::size_t active_count_ = 0;
    std::size_t peak_count_ = 0;

    static std::size_t hash_node(const NodeT<Arity>& node);
    void grow();
};

class Package {
public:
    class Root {
    public:
        Root(Package& pkg, Edge edge = {});
        ~Root();
        Root(const Root&) = delete;
        Root& operator=(const Root&) = delete;
        Root(Root&& other) noexcept;
        Root& operator=(Root&& other) noexcept;

        Edge edge() const { return edge_; }
        void set(Edge e) { edge_ = e; }
        Edge* ptr() { return &edge_; }
    private:
        Package* pkg_ = nullptr;
        Edge edge_;
    };

    explicit Package(std::size_t nqubits, PackageConfig cfg = {});

    std::size_t nqubits() const { return nqubits_; }
    ComplexTable& complex_table() { return complex_table_; }
    const ComplexTable& complex_table() const { return complex_table_; }

    Edge make_vnode(std::uint16_t q, std::array<Edge, 2> children);
    Edge make_mnode(std::uint16_t q, std::array<Edge, 4> children);

    Edge zero_state();
    Edge basis_state(std::uint64_t val);
    Edge identity();
    Edge identity(std::uint16_t q);

    Edge add_v(Edge a, Edge b);
    Edge add_m(Edge a, Edge b);
    Edge add(Edge a, Edge b);
    Edge mat_vec(Edge m, Edge v);
    Edge mat_mat(Edge a, Edge b);
    Edge adjoint(Edge m);

    Edge gate(const Mat2& u, std::uint16_t target, std::span<const Control> ctrls = {});
    Edge gate(const Mat2& u, std::uint16_t target, std::initializer_list<Control> ctrls) {
        return gate(u, target, std::span<const Control>(ctrls.begin(), ctrls.end()));
    }

    std::complex<double> amplitude(Edge v, std::uint64_t basis) const;
    std::vector<std::complex<double>> get_all_amplitudes(Edge v) const;
    std::string sample(Edge v, std::uint64_t seed = 42);

    void gc();
    void register_root(Edge* root);
    void unregister_root(Edge* root);

    Stats stats() const;
    void reset_stats();

    const VNode& vnode(NodeID id) const { return v_unique_.get(id & ~kMatrixFlag); }
    const MNode& mnode(NodeID id) const { return m_unique_.get(id & ~kMatrixFlag); }

    void check_gc();

private:
    std::size_t nqubits_;
    PackageConfig cfg_;
    ComplexTable complex_table_;

    UniqueTable<2> v_unique_;
    UniqueTable<4> m_unique_;
    std::vector<NodeID> identities_;

    struct AddCacheEntry {
        NodeID a_node = kTerminal;
        CIdx a_w = kC0;
        NodeID b_node = kTerminal;
        CIdx b_w = kC0;
        Edge result{};
    };

    struct MatVecCacheEntry {
        NodeID m_node = kTerminal;
        NodeID v_node = kTerminal;
        Edge result{};
    };

    struct MatMatCacheEntry {
        NodeID a_node = kTerminal;
        NodeID b_node = kTerminal;
        Edge result{};
    };

    struct AdjointCacheEntry {
        NodeID m_node = kTerminal;
        Edge result{};
    };

    std::size_t cache_mask_;
    std::vector<AddCacheEntry> add_cache_;
    std::vector<MatVecCacheEntry> mat_vec_cache_;
    std::vector<MatMatCacheEntry> mat_mat_cache_;
    std::vector<AdjointCacheEntry> adjoint_cache_;

    std::unordered_set<Edge*> roots_;
    mutable Stats stats_;
    std::size_t next_gc_threshold_;

    void init_identities();
    void clear_caches();

    Edge add_v_rec(Edge a, Edge b);
    Edge add_m_rec(Edge a, Edge b);
    Edge mat_vec_rec(Edge m, Edge v);
    Edge mat_mat_rec(Edge a, Edge b);
    Edge adjoint_rec(Edge m);

    void mark_vnode_rec(NodeID id);
    void mark_mnode_rec(NodeID id);
};

} // namespace qdd

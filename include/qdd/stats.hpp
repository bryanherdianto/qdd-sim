#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace qdd {

struct Stats {
    std::size_t vector_nodes = 0;
    std::size_t matrix_nodes = 0;
    std::size_t peak_vector_nodes = 0;
    std::size_t peak_matrix_nodes = 0;
    std::size_t complex_entries = 0;
    std::size_t gc_count = 0;

    std::uint64_t add_lookups = 0;
    std::uint64_t add_hits = 0;
    std::uint64_t mat_vec_lookups = 0;
    std::uint64_t mat_vec_hits = 0;
    std::uint64_t mat_mat_lookups = 0;
    std::uint64_t mat_mat_hits = 0;
    std::uint64_t adjoint_lookups = 0;
    std::uint64_t adjoint_hits = 0;

    double add_hit_ratio() const {
        return add_lookups ? static_cast<double>(add_hits) / static_cast<double>(add_lookups) : 0.0;
    }

    double mat_vec_hit_ratio() const {
        return mat_vec_lookups ? static_cast<double>(mat_vec_hits) / static_cast<double>(mat_vec_lookups) : 0.0;
    }

    double mat_mat_hit_ratio() const {
        return mat_mat_lookups ? static_cast<double>(mat_mat_hits) / static_cast<double>(mat_mat_lookups) : 0.0;
    }

    double overall_hit_ratio() const {
        std::uint64_t total_lookups = add_lookups + mat_vec_lookups + mat_mat_lookups + adjoint_lookups;
        std::uint64_t total_hits = add_hits + mat_vec_hits + mat_mat_hits + adjoint_hits;
        return total_lookups ? static_cast<double>(total_hits) / static_cast<double>(total_lookups) : 0.0;
    }
};

std::string stats_to_json(const Stats& s, double runtime_s, std::size_t peak_mem_bytes);

} // namespace qdd

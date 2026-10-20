#include "qdd/stats.hpp"

#include <sstream>

namespace qdd {

std::string stats_to_json(const Stats& s, double runtime_s, std::size_t peak_mem_bytes) {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"vector_nodes\": " << s.vector_nodes << ",\n";
    oss << "  \"matrix_nodes\": " << s.matrix_nodes << ",\n";
    oss << "  \"peak_vector_nodes\": " << s.peak_vector_nodes << ",\n";
    oss << "  \"peak_matrix_nodes\": " << s.peak_matrix_nodes << ",\n";
    oss << "  \"complex_entries\": " << s.complex_entries << ",\n";
    oss << "  \"gc_count\": " << s.gc_count << ",\n";
    oss << "  \"add_lookups\": " << s.add_lookups << ",\n";
    oss << "  \"add_hits\": " << s.add_hits << ",\n";
    oss << "  \"mat_vec_lookups\": " << s.mat_vec_lookups << ",\n";
    oss << "  \"mat_vec_hits\": " << s.mat_vec_hits << ",\n";
    oss << "  \"mat_mat_lookups\": " << s.mat_mat_lookups << ",\n";
    oss << "  \"mat_mat_hits\": " << s.mat_mat_hits << ",\n";
    oss << "  \"adjoint_lookups\": " << s.adjoint_lookups << ",\n";
    oss << "  \"adjoint_hits\": " << s.adjoint_hits << ",\n";
    oss << "  \"overall_hit_ratio\": " << s.overall_hit_ratio() << ",\n";
    oss << "  \"runtime_seconds\": " << runtime_s << ",\n";
    oss << "  \"peak_memory_bytes\": " << peak_mem_bytes << ",\n";
    oss << "  \"peak_memory_mb\": " << (static_cast<double>(peak_mem_bytes) / (1024.0 * 1024.0)) << "\n";
    oss << "}\n";
    return oss.str();
}

} // namespace qdd

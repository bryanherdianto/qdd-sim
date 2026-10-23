#include "qdd/ec.hpp"
#include "qdd/qasm.hpp"
#include "qdd/simulator.hpp"
#include "qdd/stats.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

void print_usage(const char* prog) {
    std::cout << "Usage:\n"
              << "  " << prog << " --sim <circuit.qasm> [options]\n"
              << "  " << prog << " --ec <circuit1.qasm> <circuit2.qasm> [options]\n\n"
              << "Simulation Options:\n"
              << "  --shots <N>               Number of measurement samples to draw\n"
              << "  --amplitudes              Print all final statevector amplitudes (n <= 20)\n\n"
              << "Equivalence Checking Options:\n"
              << "  --ec-strategy <strat>     Strategy: 'alternating' (default) or 'naive'\n\n"
              << "Common Options:\n"
              << "  --stats-json <file>       Write execution and memory stats to JSON file\n"
              << "  --max-nodes <N>           Maximum DD nodes allowed before error (default: unlimited)\n"
              << "  --gc-threshold <N>        Initial node count to trigger garbage collection (default: 100000)\n"
              << "  --cache-log2 <K>          Log2 size of direct-mapped compute cache (default: 18)\n"
              << "  --tol <eps>               Tolerance for complex weight interning (default: 1e-13)\n"
              << "  --quiet                   Suppress standard info prints\n"
              << "  --help                    Show this help message\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 2;
    }

    bool is_sim = false;
    bool is_ec = false;
    std::string qasm_file1;
    std::string qasm_file2;
    std::size_t shots = 0;
    bool print_amplitudes = false;
    std::string ec_strategy_str = "alternating";
    std::string stats_json_file;
    bool quiet = false;

    qdd::PackageConfig cfg;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--sim") {
            is_sim = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                qasm_file1 = argv[++i];
            } else {
                std::cerr << "Error: --sim requires a circuit file argument\n";
                return 2;
            }
        } else if (arg == "--ec") {
            is_ec = true;
            if (i + 2 < argc && argv[i + 1][0] != '-' && argv[i + 2][0] != '-') {
                qasm_file1 = argv[++i];
                qasm_file2 = argv[++i];
            } else {
                std::cerr << "Error: --ec requires two circuit file arguments\n";
                return 2;
            }
        } else if (arg == "--shots") {
            if (i + 1 < argc) shots = std::stoull(argv[++i]);
        } else if (arg == "--amplitudes") {
            print_amplitudes = true;
        } else if (arg == "--ec-strategy") {
            if (i + 1 < argc) ec_strategy_str = argv[++i];
        } else if (arg == "--stats-json") {
            if (i + 1 < argc) stats_json_file = argv[++i];
        } else if (arg == "--max-nodes") {
            if (i + 1 < argc) cfg.max_nodes = std::stoull(argv[++i]);
        } else if (arg == "--gc-threshold") {
            if (i + 1 < argc) cfg.gc_threshold = std::stoull(argv[++i]);
        } else if (arg == "--cache-log2") {
            if (i + 1 < argc) cfg.cache_log2 = std::stoull(argv[++i]);
        } else if (arg == "--tol") {
            if (i + 1 < argc) cfg.tol = std::stod(argv[++i]);
        } else if (arg == "--quiet") {
            quiet = true;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            print_usage(argv[0]);
            return 2;
        }
    }

    if (!is_sim && !is_ec) {
        std::cerr << "Error: Must specify either --sim or --ec mode\n";
        return 2;
    }

    try {
        if (is_sim) {
            auto circuit = qdd::parse_qasm_file(qasm_file1);
            if (!quiet) {
                std::cout << "[qdd-sim] Loaded circuit: " << qasm_file1
                          << " (" << circuit.nqubits << " qubits, "
                          << circuit.ops.size() << " gates)\n";
            }

            qdd::Simulator sim(cfg);
            auto res = sim.simulate(circuit);

            if (!quiet) {
                std::cout << "[qdd-sim] Simulation complete in "
                          << std::fixed << std::setprecision(4) << res.runtime_seconds << " s\n"
                          << "  Final DD nodes: " << res.stats.vector_nodes << "\n"
                          << "  Peak DD nodes:  " << res.stats.peak_vector_nodes << "\n"
                          << "  Peak RSS memory: "
                          << (static_cast<double>(res.peak_memory_bytes) / (1024.0 * 1024.0)) << " MB\n";
            }

            if (print_amplitudes && res.pkg) {
                auto amps = res.pkg->get_all_amplitudes(res.final_state);
                std::cout << "Amplitudes (basis state -> value):\n";
                std::cout << std::defaultfloat << std::setprecision(16);
                for (std::size_t i = 0; i < amps.size(); ++i) {
                    if (std::abs(amps[i]) > 1e-10) {
                        std::cout << "  |" << i << ">: " << amps[i] << " (prob = "
                                  << std::norm(amps[i]) << ")\n";
                    }
                }
            }

            if (shots > 0 && res.pkg) {
                std::unordered_map<std::string, std::size_t> counts;
                for (std::size_t s = 0; s < shots; ++s) {
                    counts[res.pkg->sample(res.final_state, s + 1)]++;
                }
                std::cout << "Measurement counts (" << shots << " shots):\n";
                for (const auto& [bitstring, cnt] : counts) {
                    std::cout << "  \"" << bitstring << "\": " << cnt << "\n";
                }
            }

            if (!stats_json_file.empty()) {
                std::ofstream out(stats_json_file);
                out << qdd::stats_to_json(res.stats, res.runtime_seconds, res.peak_memory_bytes);
            }
            return 0;
        } else {
            auto c1 = qdd::parse_qasm_file(qasm_file1);
            auto c2 = qdd::parse_qasm_file(qasm_file2);

            if (!quiet) {
                std::cout << "[qdd-sim] Equivalence checking:\n"
                          << "  Circuit 1: " << qasm_file1 << " (" << c1.nqubits << " q, " << c1.ops.size() << " g)\n"
                          << "  Circuit 2: " << qasm_file2 << " (" << c2.nqubits << " q, " << c2.ops.size() << " g)\n";
            }

            qdd::ECStrategy strat = (ec_strategy_str == "naive") ?
                                    qdd::ECStrategy::Naive : qdd::ECStrategy::Alternating;
            qdd::EquivalenceChecker checker(cfg, strat);
            auto res = checker.check(c1, c2);

            if (!quiet) {
                std::cout << "[qdd-sim] Result: ";
                if (res.status == qdd::ECStatus::Equivalent) {
                    std::cout << "EQUIVALENT\n";
                } else if (res.status == qdd::ECStatus::EquivalentUpToGlobalPhase) {
                    std::cout << "EQUIVALENT (up to global phase " << res.global_phase << " rad)\n";
                } else if (res.status == qdd::ECStatus::NotEquivalent) {
                    std::cout << "NOT EQUIVALENT\n";
                } else if (res.status == qdd::ECStatus::QubitMismatch) {
                    std::cout << "ERROR: Qubit count mismatch (" << c1.nqubits << " vs " << c2.nqubits << ")\n";
                } else if (res.status == qdd::ECStatus::NodeLimitExceeded) {
                    std::cout << "ERROR: Node limit exceeded\n";
                }
                std::cout << "  Time: " << std::fixed << std::setprecision(4)
                          << res.runtime_seconds << " s, Peak Memory: "
                          << (static_cast<double>(res.peak_memory_bytes) / (1024.0 * 1024.0)) << " MB\n";
            }

            if (!stats_json_file.empty()) {
                std::ofstream out(stats_json_file);
                out << qdd::stats_to_json(res.stats, res.runtime_seconds, res.peak_memory_bytes);
            }

            if (res.status == qdd::ECStatus::Equivalent ||
                res.status == qdd::ECStatus::EquivalentUpToGlobalPhase) {
                return 0;
            } else if (res.status == qdd::ECStatus::NotEquivalent) {
                return 1;
            } else if (res.status == qdd::ECStatus::NodeLimitExceeded) {
                return 3;
            } else {
                return 2;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << "\n";
        return 2;
    }
}

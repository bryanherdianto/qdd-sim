#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "qdd/simulator.hpp"
#include "qdd/qasm.hpp"

TEST_CASE("Simulator with GHZ and BV", "[simulator]") {
    SECTION("GHZ(3) State") {
        std::string qasm = R"(
OPENQASM 2.0;
qreg q[3];
h q[0];
cx q[0], q[1];
cx q[1], q[2];
)";
        auto circuit = qdd::parse_qasm_string(qasm);
        qdd::Simulator sim;
        auto res = sim.simulate(circuit);

        double inv_sqrt2 = 1.0 / std::sqrt(2.0);

        REQUIRE(res.amplitude(0).real() == Catch::Approx(inv_sqrt2).margin(1e-12));
        REQUIRE(res.amplitude(7).real() == Catch::Approx(inv_sqrt2).margin(1e-12));
        REQUIRE(res.amplitude(1).real() == Catch::Approx(0.0).margin(1e-12));
        REQUIRE(res.amplitude(2).real() == Catch::Approx(0.0).margin(1e-12));
    }

    SECTION("GHZ(100) Scalability") {
        qdd::Circuit circuit;
        circuit.nqubits = 100;
        circuit.qregs.emplace_back("q", 100);

        // h q[0]
        circuit.ops.push_back(qdd::GateOp{qdd::GateType::H, {0}, {}, {}});
        // cx q[i], q[i+1]
        for (std::uint16_t i = 0; i < 99; ++i) {
            circuit.ops.push_back(qdd::GateOp{
                qdd::GateType::CX,
                {static_cast<std::uint16_t>(i + 1)},
                {qdd::Control{i, true}},
                {}
            });
        }

        qdd::Simulator sim;
        auto res = sim.simulate(circuit);

        // Node count should be O(n)
        REQUIRE(res.stats.vector_nodes <= 300);
        REQUIRE(res.runtime_seconds < 1.0);
    }
}

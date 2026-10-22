#include <catch2/catch_test_macros.hpp>
#include "qdd/qasm.hpp"

TEST_CASE("QASM 2.0 Parsing", "[qasm]") {
    SECTION("Parse simple circuit") {
        std::string qasm = R"(
OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
creg c[2];
h q[0];
cx q[0], q[1];
measure q[0] -> c[0];
)";
        auto circuit = qdd::parse_qasm_string(qasm);
        REQUIRE(circuit.nqubits == 2);
        REQUIRE(circuit.qregs.size() == 1);
        REQUIRE(circuit.cregs.size() == 1);
        REQUIRE(circuit.ops.size() == 3);

        REQUIRE(circuit.ops[0].type == qdd::GateType::H);
        REQUIRE(circuit.ops[0].targets[0] == 0);

        REQUIRE(circuit.ops[1].type == qdd::GateType::CX);
        REQUIRE(circuit.ops[1].controls[0].qubit == 0);
        REQUIRE(circuit.ops[1].targets[0] == 1);

        REQUIRE(circuit.ops[2].type == qdd::GateType::Measure);
    }

    SECTION("Parse multiple registers") {
        std::string qasm = R"(
OPENQASM 2.0;
qreg a[2];
qreg b[3];
h a[1];
cx a[0], b[2];
)";
        auto circuit = qdd::parse_qasm_string(qasm);
        REQUIRE(circuit.nqubits == 5);
        REQUIRE(circuit.ops[0].targets[0] == 1);
        REQUIRE(circuit.ops[1].controls[0].qubit == 0);
        REQUIRE(circuit.ops[1].targets[0] == 4);
    }
}

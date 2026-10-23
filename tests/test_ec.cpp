#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "qdd/ec.hpp"
#include "qdd/qasm.hpp"

#include <numbers>

TEST_CASE("Equivalence Checker", "[ec]") {
    qdd::EquivalenceChecker checker;

    SECTION("Identity and equivalent circuit rewritings") {
        // CX(0, 1) is equivalent to H(1) CZ(0, 1) H(1)
        std::string qasm1 = R"(
OPENQASM 2.0;
qreg q[2];
cx q[0], q[1];
)";

        std::string qasm2 = R"(
OPENQASM 2.0;
qreg q[2];
h q[1];
cz q[0], q[1];
h q[1];
)";
        auto c1 = qdd::parse_qasm_string(qasm1);
        auto c2 = qdd::parse_qasm_string(qasm2);

        auto res = checker.check(c1, c2);
        REQUIRE(res.status == qdd::ECStatus::Equivalent);
    }

    SECTION("SWAP equivalent to 3 CX gates") {
        std::string qasm1 = R"(
OPENQASM 2.0;
qreg q[2];
swap q[0], q[1];
)";

        std::string qasm2 = R"(
OPENQASM 2.0;
qreg q[2];
cx q[0], q[1];
cx q[1], q[0];
cx q[0], q[1];
)";
        auto c1 = qdd::parse_qasm_string(qasm1);
        auto c2 = qdd::parse_qasm_string(qasm2);

        auto res = checker.check(c1, c2);
        REQUIRE(res.status == qdd::ECStatus::Equivalent);
    }

    SECTION("Non-equivalent circuits") {
        std::string qasm1 = R"(
OPENQASM 2.0;
qreg q[2];
cx q[0], q[1];
)";

        std::string qasm2 = R"(
OPENQASM 2.0;
qreg q[2];
cz q[0], q[1];
)";
        auto c1 = qdd::parse_qasm_string(qasm1);
        auto c2 = qdd::parse_qasm_string(qasm2);

        auto res = checker.check(c1, c2);
        REQUIRE(res.status == qdd::ECStatus::NotEquivalent);
    }

    SECTION("Global phase equivalence") {
        // Z gate vs RZ(pi) gate (differ by global phase e^{-i pi/2} = -i)
        std::string qasm1 = R"(
OPENQASM 2.0;
qreg q[1];
z q[0];
)";

        std::string qasm2 = R"(
OPENQASM 2.0;
qreg q[1];
rz(3.141592653589793) q[0];
)";
        auto c1 = qdd::parse_qasm_string(qasm1);
        auto c2 = qdd::parse_qasm_string(qasm2);

        auto res = checker.check(c1, c2);
        REQUIRE((res.status == qdd::ECStatus::EquivalentUpToGlobalPhase ||
                 res.status == qdd::ECStatus::Equivalent));
    }
}

#include <catch2/catch_test_macros.hpp>
#include "qdd/dd.hpp"
#include "qdd/gates.hpp"

TEST_CASE("Canonicity and Normalisation", "[normalisation]") {
    qdd::Package pkg(2);

    SECTION("Zero state normalisation") {
        auto zero = pkg.zero_state();
        REQUIRE(zero.w == qdd::kC1);
        REQUIRE(!zero.is_zero());

        // Amplitudes
        REQUIRE(pkg.amplitude(zero, 0) == std::complex<double>(1.0, 0.0));
        REQUIRE(pkg.amplitude(zero, 1) == std::complex<double>(0.0, 0.0));
        REQUIRE(pkg.amplitude(zero, 2) == std::complex<double>(0.0, 0.0));
        REQUIRE(pkg.amplitude(zero, 3) == std::complex<double>(0.0, 0.0));
    }

    SECTION("Commuting gates build identical root node") {
        // H on q0 and X on q1 commute
        auto h0 = pkg.gate(qdd::gates::H(), 0);
        auto x1 = pkg.gate(qdd::gates::X(), 1);

        // Path A: H0 then X1
        auto s0 = pkg.zero_state();
        auto s_a1 = pkg.mat_vec(h0, s0);
        auto s_a2 = pkg.mat_vec(x1, s_a1);

        // Path B: X1 then H0
        auto s_b1 = pkg.mat_vec(x1, s0);
        auto s_b2 = pkg.mat_vec(h0, s_b1);

        REQUIRE(s_a2.node == s_b2.node);
        REQUIRE(s_a2.w == s_b2.w);
    }
}

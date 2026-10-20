#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "qdd/complex_table.hpp"

#include <numbers>

TEST_CASE("ComplexTable basic interning and tolerance", "[complex_table]") {
    qdd::ComplexTable table;

    SECTION("Pre-seeded constants") {
        REQUIRE(table.value(qdd::kC0) == std::complex<double>(0.0, 0.0));
        REQUIRE(table.value(qdd::kC1) == std::complex<double>(1.0, 0.0));
        REQUIRE(table.value(qdd::kCMinus1) == std::complex<double>(-1.0, 0.0));
        REQUIRE(table.value(qdd::kCI) == std::complex<double>(0.0, 1.0));
        REQUIRE(table.value(qdd::kCMinusI) == std::complex<double>(0.0, -1.0));
    }

    SECTION("Tolerance matching within 1e-14") {
        auto idx1 = table.intern({1.0, 0.0});
        auto idx2 = table.intern({1.0 + 1e-15, 0.0});
        REQUIRE(idx1 == qdd::kC1);
        REQUIRE(idx2 == qdd::kC1);
    }

    SECTION("Different values have different indices") {
        auto idx1 = table.intern({0.5, 0.5});
        auto idx2 = table.intern({0.5, 0.6});
        REQUIRE(idx1 != idx2);
    }

    SECTION("Arithmetic operations") {
        auto half = table.intern({0.5, 0.0});
        auto two_half = table.add(half, half);
        REQUIRE(two_half == qdd::kC1);

        auto prod = table.mul(qdd::kCI, qdd::kCI);
        REQUIRE(prod == qdd::kCMinus1);

        auto conj_i = table.conj(qdd::kCI);
        REQUIRE(conj_i == qdd::kCMinusI);

        auto div_res = table.div(qdd::kC1, half);
        REQUIRE(table.value(div_res).real() == Catch::Approx(2.0));
    }
}

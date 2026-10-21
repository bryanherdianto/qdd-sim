#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "qdd/dd.hpp"
#include "qdd/gates.hpp"

TEST_CASE("Vector Operations vs Dense Reference", "[vector_ops]") {
    qdd::Package pkg(3);

    SECTION("Superposition state amplitudes") {
        // Apply H to all 3 qubits
        auto s = pkg.zero_state();
        for (std::uint16_t q = 0; q < 3; ++q) {
            auto h = pkg.gate(qdd::gates::H(), q);
            s = pkg.mat_vec(h, s);
        }

        // Each of the 8 states should have amplitude 1/sqrt(8)
        double expected = 1.0 / std::sqrt(8.0);
        for (std::uint64_t i = 0; i < 8; ++i) {
            auto amp = pkg.amplitude(s, i);
            REQUIRE(amp.real() == Catch::Approx(expected).margin(1e-12));
            REQUIRE(amp.imag() == Catch::Approx(0.0).margin(1e-12));
        }
    }

    SECTION("Vector addition") {
        auto b0 = pkg.basis_state(0);
        auto b1 = pkg.basis_state(1);

        auto sum = pkg.add(b0, b1);
        auto amp0 = pkg.amplitude(sum, 0);
        auto amp1 = pkg.amplitude(sum, 1);
        auto amp2 = pkg.amplitude(sum, 2);

        REQUIRE(amp0.real() == Catch::Approx(1.0));
        REQUIRE(amp1.real() == Catch::Approx(1.0));
        REQUIRE(amp2.real() == Catch::Approx(0.0));
    }
}

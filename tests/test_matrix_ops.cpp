#include <catch2/catch_test_macros.hpp>
#include "qdd/dd.hpp"
#include "qdd/gates.hpp"
#include <iostream>

TEST_CASE("Matrix Operations", "[matrix_ops]") {
    qdd::Package pkg(2);

    SECTION("Identity check") {
        auto id = pkg.identity();
        REQUIRE(pkg.mnode(id.node).identity);
        REQUIRE(id.w == qdd::kC1);
    }

    SECTION("U * U^dagger = I for Pauli matrices") {
        auto id = pkg.identity();

        auto x = pkg.gate(qdd::gates::X(), 0);
        auto x_adj = pkg.adjoint(x);
        auto prod_x = pkg.mat_mat(x, x_adj);
        REQUIRE(prod_x.node == id.node);
        REQUIRE(prod_x.w == id.w);

        auto y = pkg.gate(qdd::gates::Y(), 1);
        auto y_adj = pkg.adjoint(y);
        auto prod_y = pkg.mat_mat(y, y_adj);
        REQUIRE(prod_y.node == id.node);
        REQUIRE(prod_y.w == id.w);
    }

    SECTION("Hadamard squared is identity") {
        auto id = pkg.identity();
        auto h = pkg.gate(qdd::gates::H(), 0);
        auto hh = pkg.mat_mat(h, h);
        REQUIRE(hh.node == id.node);
        REQUIRE(hh.w == id.w);
    }
}

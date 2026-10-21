#include <catch2/catch_test_macros.hpp>
#include "qdd/dd.hpp"
#include "qdd/gates.hpp"

TEST_CASE("Gate Properties and Inverses", "[gates]") {
    qdd::Package pkg(3);
    auto id = pkg.identity();

    SECTION("T^8 == Identity and S^4 == Identity") {
        auto t = pkg.gate(qdd::gates::T(), 0);
        auto t2 = pkg.mat_mat(t, t);
        auto t4 = pkg.mat_mat(t2, t2);
        auto t8 = pkg.mat_mat(t4, t4);
        REQUIRE(t8.node == id.node);
        REQUIRE(t8.w == id.w);

        auto s = pkg.gate(qdd::gates::S(), 1);
        auto s2 = pkg.mat_mat(s, s);
        auto s4 = pkg.mat_mat(s2, s2);
        REQUIRE(s4.node == id.node);
        REQUIRE(s4.w == id.w);
    }

    SECTION("CX^2 == Identity") {
        auto cx = pkg.gate(qdd::gates::X(), 1, {qdd::Control{0, true}});
        auto cx2 = pkg.mat_mat(cx, cx);
        REQUIRE(cx2.node == id.node);
        REQUIRE(cx2.w == id.w);
    }

    SECTION("CCX^2 == Identity") {
        auto ccx = pkg.gate(qdd::gates::X(), 2, {qdd::Control{0, true}, qdd::Control{1, true}});
        auto ccx2 = pkg.mat_mat(ccx, ccx);
        REQUIRE(ccx2.node == id.node);
        REQUIRE(ccx2.w == id.w);
    }
}
